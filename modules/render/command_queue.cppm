module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <atomic>

export module kwik.render.command_queue;

import kwik.core.types;
import kwik.render.layer;

import std;

/**
 * @brief 帧提交元数据
 *
 * ring buffer 传递此结构，层树通过 shared_ptr 传递。
 */
export struct FrameSubmit {
    uint64_t frameId = 0;             /**< 单调递增帧序号 */
    std::shared_ptr<Layer> rootLayer; /**< 层树根（共享所有权，渲染线程只读） */
    Rect dirtyRect = {};              /**< 脏区（物理像素坐标） */
    bool structuralChange = false;    /**< true=结构变化，渲染线程需重置 GPU 状态 */
    bool needsResize = false;
    int resizeWidth = 0;
    int resizeHeight = 0;
};

/**
 * @brief 三缓冲层树队列
 *
 * 继承原三缓冲机制，将 CommandArena 替换为 Layer 树。
 * 每个槽位持有独立的层树根。
 */
export class CommandQueue {
public:
    CommandQueue();
    ~CommandQueue();

    CommandQueue(const CommandQueue &) = delete;
    CommandQueue &operator=(const CommandQueue &) = delete;

    // ── 主线程接口 ──

    /**
     * @brief 获取当前可写入槽位对应的层树根
     *
     * 返回 writeIdx 槽位的 layer root shared_ptr。
     * 主线程通过 LayerTreeBuilder::beginFrame(root, structural) 复用此层树。
     */
    std::shared_ptr<Layer> currentRootLayer();

    /**
     * @brief 获取当前可写入的帧元数据槽位
     */
    FrameSubmit &currentFrame();

    /**
     * @brief 提交当前帧
     *
     * 阻塞直到 writeIdx - releaseIdx < 3。
     * 提交后 writeIdx 前进，下一帧写入新槽位。
     */
    bool submit();

    // ── 渲染线程接口 ──

    /**
     * @brief 获取下一帧的帧元数据
     * @param block 是否阻塞等待
     * @return 成功获取返回 true
     */
    bool acquire(bool block = true);

    /** @brief 获取当前待处理帧的元数据 */
    const FrameSubmit &pendingFrame() const;

    /**
     * @brief 渲染线程：已完成该帧数据的读取
     */
    void releaseRead();

    /**
     * @brief 渲染线程：GPU 已完成该帧的处理
     */
    void releaseGPU();

    /** @brief 唤醒阻塞的线程 */
    void wake();

private:
    static constexpr size_t kMaxInFlight = 3;    // 3缓冲：录制/解析/GPU 各一槽

    FrameSubmit frames_[kMaxInFlight]; /**< 帧元数据槽位 */

    std::atomic<size_t> writeIdx_{0};   /**< 主线程独占递增 */
    std::atomic<size_t> readIdx_{0};    /**< 渲染线程独占递增 */
    std::atomic<size_t> releaseIdx_{0}; /**< 渲染线程独占递增 */

    size_t pendingIdx_ = 0;
    std::atomic<bool> stopping_{false};

    /**
     * @brief 等待当前写槽可安全写入（背压前移的核心）
     *
     * 背景：原设计背压在 submit() 内，但 currentFrame()/currentRootLayer()
     * 在 submit() 之前就返回槽位引用并开始写入——当主线程领先 3 帧时，
     * 写入的正是渲染线程尚未释放的在途帧（resize 标志被抹掉、层树被
     * use-after-free → 拉伸/闪退）。因此必须在【取槽】时先等待槽位释放。
     *
     * @return true=槽位可写；false=队列正在停止（调用方直接放弃本帧）
     */
    bool waitWritable();
};