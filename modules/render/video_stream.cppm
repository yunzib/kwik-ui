/**
 * @file video_stream.cppm
 * @brief 视频流数据层 — 生产者推帧 / 消费者取帧（引擎与渲染后端中立）
 *
 * 三线程契约（V1 推流方案批 1）：
 *   生产者线程：acquire() → 写池槽平面（零中间缓冲）→ commit()；池满即丢帧，从不阻塞
 *   UI 线程：   只读 seq()（原子）与 desc()，据此标脏
 *   渲染线程：  pinLatest() 固定最新帧拷贝进 staging，PinGuard 析构解除固定
 *
 * 本模块不持有任何 GPU 资源；GPU 侧绑定见各渲染后端的 createStreamTexture。
 * 槽内存 4096 对齐（为批 3 外部内存零拷贝导入预留），平面偏移/stride 256 对齐。
 */
module;
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module kwik.render.video_stream;

import std;

export {
/** @brief 像素格式（批 1 GPU 消费 RGBA8/BGRA8；YUV 各格式布局先行、批 2 上 GPU）*/
enum class PixelFormat : uint8_t {
    RGBA8,    // 4B/px 打包
    BGRA8,    // 4B/px 打包（Windows 采集原生序）
    I420,     // 3 平面 YUV 4:2:0（Y/U/V）
    YV12,     // 3 平面 YUV 4:2:0（Y/V/U）
    NV12,     // 2 平面 Y + UV 交错（UVUV）
    NV21,     // 2 平面 Y + VU 交错（Android 采集原生序）
    YUYV,     // 打包 4:2:2（Y0 U Y1 V，宽度须为偶数）
    UYVY,     // 打包 4:2:2（U Y0 V Y1，宽度须为偶数）
};

/** @brief YUV→RGB 色彩矩阵（批 2 着色器转色用） */
enum class YuvMatrix : uint8_t { BT601, BT709, BT2020 };

/** @brief 一帧的描述（格式 + 尺寸 + 色彩元数据 + 可选时间戳） */
struct FrameDesc {
    PixelFormat format = PixelFormat::RGBA8;
    uint32_t width = 0;
    uint32_t height = 0;
    YuvMatrix matrix = YuvMatrix::BT709;
    bool fullRange = false;     // false=limited(16-235) true=full(0-255)
    int64_t ptsUs = -1;         // 预留；显示端按最新帧，不按 pts 节奏
};

/** @brief 生产者可写的一块平面视图（data 指向池槽内，stride 为行字节数）*/
struct PlaneView {
    uint8_t *data = nullptr;
    uint32_t stride = 0;    // 行字节数
    uint32_t width = 0;     // 平面宽（texel）
    uint32_t height = 0;    // 平面高（texel）
};

/** @brief 单平面在池槽内的布局 */
struct PlaneLayout {
    size_t offset = 0;          // 距槽基址偏移（256 对齐）
    uint32_t stride = 0;        // 行字节数（256 对齐）
    uint32_t width = 0;         // 平面宽（texel）
    uint32_t height = 0;        // 平面高（texel）
    uint32_t bytesPerTexel = 0;
};

/** @brief 帧内存布局 — 纯函数，按格式推导各平面（含奇数尺寸；4:2:2 校验偶宽）*/
struct FrameLayout {
    std::array<PlaneLayout, 3> planes{};
    uint32_t planeCount = 0;
    size_t totalBytes = 0;

    /** @brief 按 FrameDesc 推导布局；4:2:2 宽度为奇数时返回 planeCount=0（非法） */
    static FrameLayout forDesc(const FrameDesc &d) {
        FrameLayout L;
        auto align256 = [](size_t v) { return (v + 255) & ~size_t(255); };
        auto cw = (d.width + 1) / 2;     // ⌈w/2⌉（4:2:0 色度宽）
        auto ch = (d.height + 1) / 2;    // ⌈h/2⌉
        switch (d.format) {
        case PixelFormat::RGBA8:
        case PixelFormat::BGRA8:
            L.planes[0] = {0, (uint32_t)align256(size_t(d.width) * 4), d.width, d.height, 4};
            L.planeCount = 1;
            break;
        case PixelFormat::I420:
        case PixelFormat::YV12:
            L.planes[0] = {0, (uint32_t)align256(d.width), d.width, d.height, 1};
            L.planes[1] = {align256(L.planes[0].stride * d.height), (uint32_t)align256(cw), (uint32_t)cw, (uint32_t)ch, 1};
            L.planes[2] = {align256(L.planes[1].offset + L.planes[1].stride * ch), (uint32_t)align256(cw), (uint32_t)cw, (uint32_t)ch, 1};
            L.planeCount = 3;
            break;
        case PixelFormat::NV12:
        case PixelFormat::NV21:
            L.planes[0] = {0, (uint32_t)align256(d.width), d.width, d.height, 1};
            L.planes[1] = {align256(L.planes[0].stride * d.height), (uint32_t)align256(size_t(cw) * 2), (uint32_t)cw, (uint32_t)ch, 2};
            L.planeCount = 2;
            break;
        case PixelFormat::YUYV:
        case PixelFormat::UYVY:
            if (d.width % 2 != 0) return {};    // 打包 4:2:2 宏像素要求偶宽
            L.planes[0] = {0, (uint32_t)align256(size_t(d.width) * 2), d.width, d.height, 2};
            L.planeCount = 1;
            break;
        }
        if (L.planeCount == 0) return {};
        const auto &last = L.planes[L.planeCount - 1];
        L.totalBytes = align256(last.offset + size_t(last.stride) * last.height);
        return L;
    }
};

class VideoStream;

/**
 * @brief 生产者写帧句柄（acquire 返回；move-only）
 *
 * planes() 给出框架分配的可写平面（写满 width×height，行距 stride）；
 * commit() 发布为最新帧（seq++）。未 commit 析构 → 槽归还池（本帧丢弃）。
 */
class FrameWriter {
public:
    FrameWriter() = default;
    ~FrameWriter();
    FrameWriter(FrameWriter &&o) noexcept;
    FrameWriter &operator=(FrameWriter &&o) noexcept;
    FrameWriter(const FrameWriter &) = delete;
    FrameWriter &operator=(const FrameWriter &) = delete;

    /** @brief false = 池满（全忙）本帧丢弃——生产者应跳过，勿阻塞等待 */
    explicit operator bool() const { return stream_ != nullptr; }

    /** @brief 框架分配的可写平面（仅 Writing 期间有效） */
    std::span<const PlaneView> planes() const;

    /** @brief 发布为最新帧（上一未被固定的 Ready 帧随之作废——最新帧优先） */
    void commit();

private:
    friend class VideoStream;
    FrameWriter(VideoStream *s, size_t idx) : stream_(s), idx_(idx) {}
    VideoStream *stream_ = nullptr;
    size_t idx_ = 0;
    bool committed_ = false;
};

/**
 * @brief 消费端固定句柄（pinLatest 返回；move-only）
 *
 * 存活期间对应池槽不被回收复用；析构解除固定（此时若已非最新帧则槽归还）。
 */
class PinGuard {
public:
    PinGuard() = default;
    ~PinGuard();
    PinGuard(PinGuard &&o) noexcept;
    PinGuard &operator=(PinGuard &&o) noexcept;
    PinGuard(const PinGuard &) = delete;
    PinGuard &operator=(const PinGuard &) = delete;

    explicit operator bool() const { return stream_ != nullptr; }
    /** @brief 被固定帧的平面（只读） */
    std::span<const PlaneView> planes() const;
    /** @brief 被固定帧的描述 */
    const FrameDesc &desc() const;
    /** @brief 被固定帧的序号（== pin 时刻的 stream->seq()） */
    uint64_t seq() const { return seq_; }

private:
    friend class VideoStream;
    PinGuard(VideoStream *s, size_t idx, uint64_t seq) : stream_(s), idx_(idx), seq_(seq) {}
    VideoStream *stream_ = nullptr;
    size_t idx_ = 0;
    uint64_t seq_ = 0;
};

/**
 * @brief 命名视频流 — 帧池 + 最新帧指针（生产者/多消费者）
 *
 * 线程约定：单生产者（acquire/push 仅生产者线程）；消费者（pinLatest）
 * 可多个（渲染线程 × 窗口数）。互斥锁只护槽状态切换，锁内不做 memcpy。
 */
class VideoStream {
public:
    /**
     * @param poolSlots 池槽数。每窗口渲染线程每帧最多固定 1 帧存活到下帧，
     *        多窗口显示同一路流时建议 ≥ 2 + 窗口数（默认 4 覆盖双窗口）
     */
    explicit VideoStream(uint32_t poolSlots = 4);
    ~VideoStream();
    VideoStream(const VideoStream &) = delete;
    VideoStream &operator=(const VideoStream &) = delete;

    /** @brief 取槽开始写帧（生产者线程；池满返回空 writer，计一次丢帧） */
    FrameWriter acquire(const FrameDesc &d);

    /**
     * @brief 便捷推帧：acquire + 按行拷贝源平面 + commit（在生产者线程执行）
     * @param src 源平面视图；src[i].stride 为源行距（可与池 stride 不同），
     *            源/目标行内容字节数取 min(源stride, 目标stride) 逐行拷贝
     */
    void push(const FrameDesc &d, std::span<const PlaneView> src);

    /** @brief 最新帧序号（commit 递增；UI 线程轮询判变） */
    uint64_t seq() const noexcept { return seq_.load(std::memory_order_acquire); }

    /** @brief 最新帧描述（从未有帧时返回全默认值） */
    FrameDesc desc() const;

    /** @brief 累计丢帧数（池满丢弃） */
    uint64_t droppedFrames() const noexcept { return dropped_.load(std::memory_order_relaxed); }

    /** @brief 固定最新帧（消费端；空 = 尚无任何帧） */
    PinGuard pinLatest();

private:
    friend class FrameWriter;
    friend class PinGuard;
    struct Slot {
        enum class St : uint8_t { Free, Writing, Ready };
        St st = St::Free;
        uint32_t pins = 0;                 // 消费端固定计数（仅 Ready 态有意义）
        uint8_t *base = nullptr;           // 4096 对齐槽内存（alignedAlloc 分配）
        size_t capBytes = 0;               // 已分配容量
        FrameDesc desc;                    // 本槽帧描述（Writing 起有效）
        std::array<PlaneView, 3> views{};  // 按 FrameLayout 生成的可写视图
    };

    void commitSlot(size_t idx);       // FrameWriter::commit 落点（锁内切态，锁外 seq++）
    void abortWrite(size_t idx);       // 未 commit 的 writer 析构 → 槽归还
    void unpin(size_t idx);            // PinGuard 析构落点
    /** @brief 槽 idx 的平面视图（Writing/被固定期间稳定，无需加锁读） */
    std::span<const PlaneView> planesFor(size_t idx) const;
    /** @brief 槽 idx 的帧描述（同上） */
    const FrameDesc &descFor(size_t idx) const;

    mutable std::mutex mtx_;
    std::vector<Slot> slots_;
    int latest_ = -1;                            // 最新 Ready 槽下标（-1 = 无帧）
    bool hasFrame_ = false;                      // 曾 commit 过（desc 可读）
    FrameDesc latestDesc_{};                     // 最新帧描述（mtx_ 护）
    std::atomic<uint64_t> seq_{0};               // 帧序号（release 发布/acquire 读取）
    std::atomic<uint64_t> dropped_{0};
};

/** @brief 创建命名流并注册（同名覆盖旧条目；旧流的已有消费者不受影响） */
std::shared_ptr<VideoStream> createVideoStream(std::string name, uint32_t poolSlots = 4);
/** @brief 按名查找（未注册返回空指针；返回 shared_ptr 延长流寿命） */
std::shared_ptr<VideoStream> findVideoStream(std::string_view name);
/** @brief 除名（流对象由持有者续命；显示端持 shared_ptr 显示最后一帧不悬空） */
void removeVideoStream(std::string_view name);
}    // export
