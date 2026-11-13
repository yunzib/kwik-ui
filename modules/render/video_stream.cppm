/**
 * @file video_stream.cppm
 * @brief 视频流数据层 — 生产者推帧 / 消费者取帧（引擎与渲染后端中立）
 *
 * 三线程契约：
 *   生产者线程：acquire() → 写池槽平面（零中间缓冲）→ commit()；池满即丢帧，从不阻塞
 *   UI 线程：   只读 seq()（原子）、desc()、fps()，据此标脏/读数
 *   渲染线程：  pinLatest() 固定最新帧，或拷入 staging（拷贝路径），或把槽内存
 *               块直接导入为 GPU 拷贝源（零拷贝路径，批 3）——后者须把固定
 *               持有到 GPU 读完（fence），故 PinGuard 可移动、可跨帧持有
 *
 * 本模块不持有任何 GPU 资源；GPU 侧绑定见各渲染后端的 createStreamTexture。
 * 槽内存为 MemoryBlock：64KiB 对齐、整块独占（满足 VK_EXT_external_memory_host
 * 导入的地址/尺寸对齐，且导入范围不与他人分配共页）；平面偏移/stride 256 对齐。
 */
export module kwik.render.video_stream;

import std;

export {
/** @brief 像素格式（批 1 起 RGBA8/BGRA8 直通；批 2 起 YUV 各格式经着色器转色）*/
enum class PixelFormat : std::uint8_t {
    RGBA8,    // 4B/px 打包
    BGRA8,    // 4B/px 打包（Windows 采集原生序）
    I420,     // 3 平面 YUV 4:2:0（Y/U/V）
    YV12,     // 3 平面 YUV 4:2:0（Y/V/U）
    NV12,     // 2 平面 Y + UV 交错（UVUV）
    NV21,     // 2 平面 Y + VU 交错（Android 采集原生序）
    YUYV,     // 打包 4:2:2（Y0 U Y1 V，宽度须为偶数）
    UYVY,     // 打包 4:2:2（U Y0 V Y1，宽度须为偶数）
};

/** @brief YUV→RGB 色彩矩阵（着色器转色；BT.2020 为非恒定亮度版本） */
enum class YuvMatrix : std::uint8_t { BT601, BT709, BT2020 };

/** @brief 一帧的描述（格式 + 尺寸 + 色彩元数据 + 可选时间戳） */
struct FrameDesc {
    PixelFormat format = PixelFormat::RGBA8;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    YuvMatrix matrix = YuvMatrix::BT709;
    bool fullRange = false;     // false=limited(16-235) true=full(0-255)
    std::int64_t ptsUs = -1;         // 预留；显示端按最新帧，不按 pts 节奏
};

/** @brief 生产者可写的一块平面视图（data 指向池槽内，stride 为行字节数）*/
struct PlaneView {
    std::uint8_t *data = nullptr;
    std::uint32_t stride = 0;    // 行字节数
    std::uint32_t width = 0;     // 平面宽（texel）
    std::uint32_t height = 0;    // 平面高（texel）
};

/** @brief 单平面在池槽内的布局 */
struct PlaneLayout {
    std::size_t offset = 0;          // 距槽基址偏移（256 对齐）
    std::uint32_t stride = 0;        // 行字节数（256 对齐）
    std::uint32_t width = 0;         // 平面宽（texel）
    std::uint32_t height = 0;        // 平面高（texel）
    std::uint32_t bytesPerTexel = 0;
};

/** @brief 帧内存布局 — 纯函数，按格式推导各平面（含奇数尺寸；4:2:2 校验偶宽）*/
struct FrameLayout {
    std::array<PlaneLayout, 3> planes{};
    std::uint32_t planeCount = 0;
    std::size_t totalBytes = 0;

    /** @brief 按 FrameDesc 推导布局；4:2:2 宽度为奇数时返回 planeCount=0（非法） */
    static FrameLayout forDesc(const FrameDesc &d) {
        FrameLayout L;
        auto align256 = [](std::size_t v) { return (v + 255) & ~std::size_t(255); };
        auto cw = (d.width + 1) / 2;     // ⌈w/2⌉（4:2:0 色度宽）
        auto ch = (d.height + 1) / 2;    // ⌈h/2⌉
        switch (d.format) {
        case PixelFormat::RGBA8:
        case PixelFormat::BGRA8:
            L.planes[0] = {0, (std::uint32_t)align256(std::size_t(d.width) * 4), d.width, d.height, 4};
            L.planeCount = 1;
            break;
        case PixelFormat::I420:
        case PixelFormat::YV12:
            L.planes[0] = {0, (std::uint32_t)align256(d.width), d.width, d.height, 1};
            L.planes[1] = {align256(L.planes[0].stride * d.height), (std::uint32_t)align256(cw), (std::uint32_t)cw, (std::uint32_t)ch, 1};
            L.planes[2] = {align256(L.planes[1].offset + L.planes[1].stride * ch), (std::uint32_t)align256(cw), (std::uint32_t)cw, (std::uint32_t)ch, 1};
            L.planeCount = 3;
            break;
        case PixelFormat::NV12:
        case PixelFormat::NV21:
            L.planes[0] = {0, (std::uint32_t)align256(d.width), d.width, d.height, 1};
            L.planes[1] = {align256(L.planes[0].stride * d.height), (std::uint32_t)align256(std::size_t(cw) * 2), (std::uint32_t)cw, (std::uint32_t)ch, 2};
            L.planeCount = 2;
            break;
        case PixelFormat::YUYV:
        case PixelFormat::UYVY:
            if (d.width % 2 != 0) return {};    // 打包 4:2:2 宏像素要求偶宽
            L.planes[0] = {0, (std::uint32_t)align256(std::size_t(d.width) * 2), d.width, d.height, 2};
            L.planeCount = 1;
            break;
        }
        if (L.planeCount == 0) return {};
        const auto &last = L.planes[L.planeCount - 1];
        L.totalBytes = align256(last.offset + std::size_t(last.stride) * last.height);
        return L;
    }
};

/** @brief GPU 采样平面的纹素格式（后端中立；Vulkan 映射为同名 UNORM 格式） */
enum class TexelFormat : std::uint8_t { R8, RG8, RGBA8, BGRA8 };

/** @brief 一张 GPU 采样平面：取帧内存第 srcPlane 平面，按 texelBytes 字节/纹素重解释上传 */
struct TexturePlane {
    TexelFormat format = TexelFormat::RGBA8;
    std::uint32_t width = 0;         // 纹素宽
    std::uint32_t height = 0;        // 纹素高
    std::uint32_t srcPlane = 0;      // 来源 FrameLayout 平面下标
    std::uint32_t texelBytes = 4;    // 每纹素字节（拷贝行长 = 源 stride / texelBytes）
};

/**
 * @brief 帧 → GPU 采样平面规划（纯函数，后端中立）
 *
 * RGB 单平面直通采样；三平面 / 双平面逐平面一张；打包 4:2:2 同一块内存
 * 重解释为两张——亮度 RG8 (w×h，每纹素 = 一个像素的 Y + 半个色度) +
 * 色度 RGBA8 ((w/2)×h，每纹素 = 一个宏像素 Y0 U Y1 V)，两者都可直接线性
 * 过滤（免着色器按奇偶取纹素）。非法描述（4:2:2 奇宽等）count = 0。
 */
struct TexturePlan {
    std::array<TexturePlane, 3> planes{};
    std::uint32_t count = 0;
    bool yuv = false;    // true = 需 YUV 着色器转色；false = RGB 直采样

    static TexturePlan forDesc(const FrameDesc &d) {
        TexturePlan P;
        const FrameLayout L = FrameLayout::forDesc(d);
        if (L.planeCount == 0) return {};
        auto plane = [&](TexelFormat f, std::uint32_t src, std::uint32_t texelBytes, std::uint32_t w) {
            P.planes[P.count++] = TexturePlane{f, w, L.planes[src].height, src, texelBytes};
        };
        switch (d.format) {
        case PixelFormat::RGBA8: plane(TexelFormat::RGBA8, 0, 4, L.planes[0].width); break;
        case PixelFormat::BGRA8: plane(TexelFormat::BGRA8, 0, 4, L.planes[0].width); break;
        case PixelFormat::I420:
        case PixelFormat::YV12:    // 平面序差异（U/V 互换）由着色器 mode 位处理
            for (std::uint32_t p = 0; p < 3; ++p) plane(TexelFormat::R8, p, 1, L.planes[p].width);
            P.yuv = true;
            break;
        case PixelFormat::NV12:
        case PixelFormat::NV21:
            plane(TexelFormat::R8, 0, 1, L.planes[0].width);
            plane(TexelFormat::RG8, 1, 2, L.planes[1].width);
            P.yuv = true;
            break;
        case PixelFormat::YUYV:
        case PixelFormat::UYVY:
            plane(TexelFormat::RG8, 0, 2, L.planes[0].width);          // 亮度视图
            plane(TexelFormat::RGBA8, 0, 4, L.planes[0].width / 2);    // 宏像素色度视图
            P.yuv = true;
            break;
        }
        return P;
    }
};

class VideoStream;

/**
 * @brief 池槽内存块（零拷贝导入单元）
 *
 * 64KiB 对齐、尺寸按 64KiB 向上取整、整块独占（Windows VirtualAlloc /
 * POSIX mmap）——覆盖各驱动 minImportedHostPointerAlignment（常见 4KiB /
 * 64KiB），且导入页内无他人数据。块不可复制；对象地址即后端导入缓存键。
 *
 * 生命周期：槽扩容/缩容换块时旧块并不立即释放——标记退役（retired），由
 * 持有的 shared_ptr 续命至最后一个使用者（池槽、PinGuard、后端导入缓存）
 * 放手；后端见 retired() 即释放其导入对象，块随之析构。
 */
class MemoryBlock {
public:
    /** @brief 分配 bytes 字节（向上取整到 64KiB）；失败时 data()==nullptr */
    explicit MemoryBlock(std::size_t bytes);
    ~MemoryBlock();
    MemoryBlock(const MemoryBlock &) = delete;
    MemoryBlock &operator=(const MemoryBlock &) = delete;

    static constexpr std::size_t kAlignment = 64 * 1024;    // 导入对齐（≥ 主流驱动最小导入对齐）

    std::uint8_t *data() const { return data_; }
    std::size_t size() const { return size_; }
    /** @brief 池已弃用本块（换块后标记）——导入缓存据此释放 GPU 侧导入对象 */
    bool retired() const { return retired_.load(std::memory_order_acquire); }
    void markRetired() { retired_.store(true, std::memory_order_release); }

private:
    std::uint8_t *data_ = nullptr;
    std::size_t size_ = 0;
    std::atomic<bool> retired_{false};
};

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
    FrameWriter(VideoStream *s, std::size_t idx) : stream_(s), idx_(idx) {}
    VideoStream *stream_ = nullptr;
    std::size_t idx_ = 0;
    bool committed_ = false;
};

/**
 * @brief 消费端固定句柄（pinLatest 返回；move-only）
 *
 * 存活期间对应池槽不被回收复用；析构解除固定（此时若已非最新帧则槽归还）。
 * 可移动、可跨帧持有——零拷贝路径把它保留到 GPU 读完该槽内存为止（fence）。
 * 句柄共享持有其流（shared_ptr），流对象不早于任何句柄析构。
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
    std::uint64_t seq() const { return seq_; }
    /** @brief 被固定帧所在的内存块（零拷贝导入源；平面地址均落在其内） */
    const std::shared_ptr<MemoryBlock> &block() const;

private:
    friend class VideoStream;
    PinGuard(std::shared_ptr<VideoStream> s, std::size_t idx, std::uint64_t seq) : stream_(std::move(s)), idx_(idx), seq_(seq) {}
    std::shared_ptr<VideoStream> stream_;
    std::size_t idx_ = 0;
    std::uint64_t seq_ = 0;
};

/**
 * @brief 命名视频流 — 帧池 + 最新帧指针（生产者/多消费者）
 *
 * 线程约定：单生产者（acquire/push 仅生产者线程）；消费者（pinLatest）
 * 可多个（渲染线程 × 窗口数）。互斥锁只护槽状态切换，锁内不做 memcpy。
 * 须经 shared_ptr 持有（createVideoStream / make_shared）——pinLatest 的
 * 句柄共享持有流本身。
 */
class VideoStream : public std::enable_shared_from_this<VideoStream> {
public:
    /** @brief 槽数上限（槽数组定长、建流即构造，扩容只启用不挪动——无锁读路径安全） */
    static constexpr std::uint32_t kMaxSlots = 16;

    /**
     * @param poolSlots 池槽数（夹到 [2, kMaxSlots]）。拷贝路径每消费端仅瞬时
     *        固定；零拷贝路径每消费端最多持有约在飞帧数个槽到 GPU 读完——
     *        后端会经 reserveConsumerPins 自动补足，宿主无需按路径手调
     */
    explicit VideoStream(std::uint32_t poolSlots = 4);
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
    std::uint64_t seq() const noexcept { return seq_.load(std::memory_order_acquire); }

    /** @brief 最新帧描述（从未有帧时返回全默认值） */
    FrameDesc desc() const;

    /** @brief 累计丢帧数（池满丢弃） */
    std::uint64_t droppedFrames() const noexcept { return dropped_.load(std::memory_order_relaxed); }

    /** @brief 近期提交帧率（最近 32 次提交的平均间隔；任意线程）——尚无两帧或
     *         超过 1 秒无新帧（生产者停推）时为 0 */
    float fps() const { return fpsAt(std::chrono::steady_clock::now()); }
    /** @brief fps() 的取时可注入版本（now 为读数时刻；行为锁用） */
    float fpsAt(std::chrono::steady_clock::time_point now) const;

    /** @brief 当前启用的池槽数 */
    std::uint32_t slotCount() const;

    /**
     * @brief 消费端登记长时持有需求（后端调用：零拷贝路径每窗口最多持有
     *        n 个槽到 GPU 读完）。按全部登记之和扩容池槽（不超过 kMaxSlots），
     *        保证生产者在消费端持有期间仍有槽可写。须与 releaseConsumerPins 配对
     */
    void reserveConsumerPins(std::uint32_t n);
    /** @brief 撤销 reserveConsumerPins 的登记（已启用的槽不回收） */
    void releaseConsumerPins(std::uint32_t n);

    /** @brief 固定最新帧（消费端；空 = 尚无任何帧） */
    PinGuard pinLatest();

private:
    friend class FrameWriter;
    friend class PinGuard;
    struct Slot {
        enum class St : std::uint8_t { Free, Writing, Ready };
        St st = St::Free;
        std::uint32_t pins = 0;                    // 消费端固定计数（仅 Ready 态有意义）
        std::shared_ptr<MemoryBlock> block;   // 槽内存（换块时旧块标记退役、由持有者续命）
        FrameDesc desc;                       // 本槽帧描述（Writing 起有效）
        std::array<PlaneView, 3> views{};     // 按 FrameLayout 生成的可写视图
    };

    void commitSlot(std::size_t idx);       // FrameWriter::commit 落点（锁内切态，锁外 seq++）
    void abortWrite(std::size_t idx);       // 未 commit 的 writer 析构 → 槽归还
    void unpin(std::size_t idx);            // PinGuard 析构落点
    /** @brief 槽 idx 的平面视图（Writing/被固定期间稳定，无需加锁读） */
    std::span<const PlaneView> planesFor(std::size_t idx) const;
    /** @brief 槽 idx 的帧描述（同上） */
    const FrameDesc &descFor(std::size_t idx) const;
    /** @brief 槽 idx 的内存块（同上） */
    const std::shared_ptr<MemoryBlock> &blockFor(std::size_t idx) const;

    mutable std::mutex mtx_;
    std::array<Slot, kMaxSlots> slots_{};        // 定长：扩容只增 slotCount_，元素地址恒定
    std::uint32_t slotCount_ = 0;                     // 当前启用槽数（mtx_ 护）
    std::uint32_t basePoolSlots_ = 0;                 // 构造时请求的槽数（扩容基数）
    std::uint32_t consumerPins_ = 0;                  // 消费端登记的长时持有总数（mtx_ 护）
    int latest_ = -1;                            // 最新 Ready 槽下标（-1 = 无帧）
    bool hasFrame_ = false;                      // 曾 commit 过（desc 可读）
    FrameDesc latestDesc_{};                     // 最新帧描述（mtx_ 护）
    std::atomic<std::uint64_t> seq_{0};               // 帧序号（release 发布/acquire 读取）
    std::atomic<std::uint64_t> dropped_{0};
    // 帧率：最近 kFpsRing 次提交时刻的环（mtx_ 护）
    static constexpr std::uint32_t kFpsRing = 32;
    std::array<std::chrono::steady_clock::time_point, kFpsRing> commitTimes_{};
    std::uint64_t commitCount_ = 0;
};

/** @brief 创建命名流并注册（同名覆盖旧条目；旧流的已有消费者不受影响） */
std::shared_ptr<VideoStream> createVideoStream(std::string name, std::uint32_t poolSlots = 4);
/** @brief 按名查找（未注册返回空指针；返回 shared_ptr 延长流寿命） */
std::shared_ptr<VideoStream> findVideoStream(std::string_view name);
/** @brief 除名（流对象由持有者续命；显示端持 shared_ptr 显示最后一帧不悬空） */
void removeVideoStream(std::string_view name);
}    // export
