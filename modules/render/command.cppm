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
    Clear,             // clear()
    FillRect,          // drawRect()
    FillRoundedRect,   // drawRoundedRect()
    StrokeRoundedRect, // drawRoundedRectStroke()
    DrawShadow,        // drawShadow()
    SaveState,         // save()
    RestoreState,      // restore()
    Translate,         // translate()
    Scale,             // scale()
    SetOpacity,        // setOpacity()
    ClipRoundedRect,   // clipRoundedRect()
    ResetClip,         // resetClip()
    BeginFrame,        // beginFrame()
    EndFrame,          // endFrame()
    Present,           // present()
    Resize             // resize()
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
 * @brief 绘制字形命令
 */
export struct DrawGlyphCmd {
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
                 SaveStateCmd,    // 空结构体，仅作为标记
                 RestoreStateCmd, // 空结构体，仅作为标记
                 TranslateCmd, ScaleCmd, SetOpacityCmd, ClipRoundedRectCmd,
                 ResetClipCmd,  // 空结构体，仅作为标记
                 BeginFrameCmd, // 空结构体，仅作为标记
                 EndFrameCmd,   // 空结构体，仅作为标记
                 PresentCmd,    // 空结构体，仅作为标记
                 ResizeCmd>;

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

private:
    std::vector<Command> commands_;
};

/**
 * @brief 命令队列
 *
 * 线程安全的双缓冲队列，支持主线程提交和渲染线程消费
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
     * @brief 设置最大队列深度（防止内存无限增长）
     */
    void setMaxDepth(size_t maxDepth);

    /**
     * @brief 唤醒所有阻塞的 acquire() 调用（用于停止时）
     */
    void wake();

private:
    // 双缓冲：一个用于当前帧记录，一个用于渲染线程处理
    CommandBuffer buffers_[2];
    CommandBuffer *currentBuffer_ = &buffers_[0];
    CommandBuffer *pendingBuffer_ = &buffers_[1];

    // 已提交但未处理的帧队列
    std::queue<CommandBuffer *> submittedQueue_;

    // 同步原语
    mutable std::mutex mutex_;
    std::condition_variable cv_;

    // 队列限制
    size_t maxDepth_ = 3;   // 最多缓存3帧
    bool stopping_ = false; // 停止标志
};
