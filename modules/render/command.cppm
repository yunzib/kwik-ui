module;

#include <cstddef>
#include <cstdint>

export module kwik.render.command;

import kwik.core.types;

import std;

/**
 * @brief 命令类型枚举
 *
 * 所有可能的渲染命令，对应Graphics类中的方法
 */
export enum class CommandType {
    Clear,                // clear()
    FillRect,             // drawRect()
    FillRoundedRect,      // drawRoundedRect()
    StrokeRoundedRect,    // drawRoundedRectStroke()
    DrawShadow,           // drawShadow()
    SaveState,            // save()
    RestoreState,         // restore()
    Translate,            // translate()
    Scale,                // scale()
    SetOpacity,           // setOpacity()
    ClipRoundedRect,      // clipRoundedRect()
    ResetClip,            // resetClip()
    BeginFrame,           // beginFrame()
    EndFrame,             // endFrame()
    Present,              // present()
    Resize,               // resize()
    DrawImage             // drawImage()
};

/**
 * @brief 清除命令
 */
export struct ClearCmd {
    Color color;
};

/**
 * @brief 填充矩形命令
 */
export struct FillRectCmd {
    Rect rect;
    Color color;
};

/**
 * @brief 填充圆角矩形命令
 */
export struct FillRoundedRectCmd {
    Rect rect;
    float radius;
    Color color;
};

/**
 * @brief 描边圆角矩形命令
 */
export struct StrokeRoundedRectCmd {
    Rect rect;
    float radius;
    Color color;
    float strokeWidth;
};

/**
 * @brief 绘制阴影命令
 */
export struct DrawShadowCmd {
    Rect rect;
    float radius;
    Shadow shadow;
};

/**
 * @brief 绘制图像命令
 *
 * 携带 Vulkan 纹理句柄、目标矩形和透明度。
 * 纹理由 VulkanBackend::createImageTexture() 创建，destroyImageTexture() 释放。
 */
export struct DrawImageCmd {
    uint32_t textureId;    // GPU 纹理句柄
    Rect rect;             // 绘制位置和尺寸 (逻辑坐标, 已变换)
    float opacity;         // 绘制透明度 (0.0-1.0)
    float cornerRadius;    //  图片圆角半径 (0=直角)
};

/**
 * @brief 绘制字形命令
 */
export struct DrawGlyphCmd {
    FontId fontId;
    uint32_t glyphIndex;
    float x;
    float y;
    float width;
    float height;
    float uvLeft;
    float uvTop;
    float uvRight;
    float uvBottom;
    Color color;
};

/**
 * @brief 平移变换命令
 */
export struct TranslateCmd {
    float dx;
    float dy;
};

/**
 * @brief 缩放变换命令
 */
export struct ScaleCmd {
    float sx;
    float sy;
};

/**
 * @brief 设置透明度命令
 */
export struct SetOpacityCmd {
    float opacity;
};

/**
 * @brief 圆角矩形裁剪命令
 */
export struct ClipRoundedRectCmd {
    Rect rect;
    float radius;
};

/**
 * @brief 调整尺寸命令
 */
export struct ResizeCmd {
    int width;
    int height;
};

// 空命令结构体定义（仅作为标记）
export struct SaveStateCmd {};
export struct RestoreStateCmd {};
export struct ResetClipCmd {};
export struct BeginFrameCmd {};
export struct EndFrameCmd {};
export struct PresentCmd {};

/**
 * @brief 命令变体类型
 *
 * 所有可能的命令类型的联合，使用std::variant实现
 */
export using Command =
    std::variant<ClearCmd, FillRectCmd, FillRoundedRectCmd, StrokeRoundedRectCmd, DrawShadowCmd, DrawGlyphCmd,
                 SaveStateCmd,       // 空结构体，仅作为标记
                 RestoreStateCmd,    // 空结构体，仅作为标记
                 TranslateCmd, ScaleCmd, SetOpacityCmd, ClipRoundedRectCmd,
                 ResetClipCmd,     // 空结构体，仅作为标记
                 BeginFrameCmd,    // 空结构体，仅作为标记
                 EndFrameCmd,      // 空结构体，仅作为标记
                 PresentCmd,       // 空结构体，仅作为标记
                 ResizeCmd,        // resize()
                 DrawImageCmd>;    // drawImage()

/**
 * @brief 命令缓冲区
 *
 * 存储一帧的所有命令，由主线程填充，渲染线程消费
 */
export class CommandBuffer {
public:
    CommandBuffer() = default;

    /**
     * @brief 清空命令缓冲区
     */
    void clear();

    /**
     * @brief 添加命令到缓冲区
     */
    void add(Command cmd);

    /**
     * @brief 获取命令数量
     */
    size_t size() const;

    /**
     * @brief 检查是否为空
     */
    bool empty() const;

    /**
     * @brief 获取命令迭代器（用于遍历）
     */
    const std::vector<Command> &commands() const;

    /**
     * @brief 交换两个命令缓冲区的内容
     */
    void swap(CommandBuffer &other);

    /**
     * @brief 设置本帧脏矩形
     * @param r 脏区域 (逻辑坐标，由主线程设置)
     */
    void setDirtyRect(const Rect &r) {
        dirtyRect_ = r;
    }
    /**
     * @brief 获取本帧脏矩形
     */
    Rect dirtyRect() const {
        return dirtyRect_;
    }

private:
    std::vector<Command> commands_;
    Rect dirtyRect_ = {};
};

/**
 * @brief 命令队列 — SPSC 无锁环形缓冲区
 *
 * 主线程（生产者）写入命令，渲染线程（消费者）读取执行。
 * 8 槽环形缓冲区，通过原子索引保证单槽所有权，永不释放内存。
 * 仅 atomic notify/wait 用于阻塞，零 mutex。
 */
export class CommandQueue {
public:
    CommandQueue();
    ~CommandQueue();
    // 禁用拷贝
    CommandQueue(const CommandQueue &) = delete;
    CommandQueue &operator=(const CommandQueue &) = delete;
    /**
     * @brief 获取当前帧的命令缓冲区（用于主线程记录命令）
     */
    CommandBuffer &currentBuffer();
    /**
     * @brief 提交当前帧的命令缓冲区到队列
     *
     * @return 提交是否成功（队列未满）
     */
    bool submit();
    /**
     * @brief 获取下一帧的命令缓冲区（渲染线程消费）
     *
     * @param block 是否阻塞等待
     * @return 成功获取返回true，队列为空返回false
     */
    bool acquire(bool block = true);
    /**
     * @brief 获取当前待处理的命令缓冲区（渲染线程使用）
     */
    const CommandBuffer &pendingBuffer() const;
    /**
     * @brief 释放已处理的命令缓冲区（渲染线程使用）
     */
    void release();
    /**
     * @brief 获取队列深度（已提交但未处理的帧数）
     */
    size_t depth() const;
    /**
     * @brief 清空队列
     */
    void clear();
    /**
     * @brief 唤醒所有阻塞的 acquire() 调用（用于停止时）
     */
    void wake();

private:
    // ── SPSC 环形缓冲区，8 槽（2的幂） ──
    static constexpr size_t kRingSize = 8;
    static constexpr size_t kMask = kRingSize - 1;
    // 环形槽：永远不释放内存，只通过读写索引轮转
    CommandBuffer buffers_[kRingSize];
    // ── 原子读写索引 ──
    // writeIdx_: 仅主线程递增 (relaxed 写入, release 发布)
    // readIdx_:  仅渲染线程递增 (relaxed 写入, release 发布)
    // writeIdx_ - readIdx_ = 队列深度 (无符号减法天然处理回绕)
    std::atomic<size_t> writeIdx_{0};
    std::atomic<size_t> readIdx_{0};
    // acquire() 后锁定的槽位索引，供 pendingBuffer() 返回
    size_t pendingIdx_ = 0;
    // 停止标志：唤醒所有等待线程，安全退出
    std::atomic<bool> stopping_{false};
};
