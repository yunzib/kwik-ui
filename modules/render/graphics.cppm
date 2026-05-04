module;

export module kwik.render.graphics;

import kwik.core.types;
import kwik.render.command;
import kwik.render.backend;

import std;

/**
 * @brief 图形渲染核心类
 *
 * 提供高层绘图 API（带变换、全局透明度、裁剪栈）。
 * 所有绘制操作被记录为命令，由渲染线程异步执行。
 */
export class Graphics {
public:
    /**
     * @brief 构造函数（旧版本，用于兼容性）
     * @deprecated 使用新的构造函数，接收 CommandBuffer*
     */
    Graphics(BackendType backend, int width, int height);

    /**
     * @brief 构造函数（新版本）
     * @param commandBuffer 命令缓冲区指针，用于记录绘制命令
     */
    explicit Graphics(CommandBuffer *commandBuffer = nullptr);

    ~Graphics();

    Graphics(const Graphics &) = delete;
    Graphics &operator=(const Graphics &) = delete;
    Graphics(Graphics &&) noexcept;
    Graphics &operator=(Graphics &&) noexcept;

    // 状态管理
    void save();
    void restore();
    void translate(float dx, float dy);
    void scale(float sx, float sy);
    void setOpacity(float opacity);

    // 裁剪
    void clipRoundedRect(const Rect &rect, float radius);
    void resetClip();

    // 绘制命令
    void clear(const Color &color);
    void drawRect(const Rect &rect, const Color &color);
    void drawRoundedRect(const Rect &rect, float radius, const Color &color);
    void drawRoundedRectStroke(const Rect &rect, float radius, const Color &color, float strokeWidth);
    void drawShadow(const Rect &rect, float radius, const Shadow &shadow);

    // 帧控制（命令记录）
    void beginFrame();
    void endFrame();
    void present();
    void resize(int width, int height);

    /**
     * @brief 设置命令缓冲区
     * @param commandBuffer 命令缓冲区指针
     */
    void setCommandBuffer(CommandBuffer *commandBuffer);

    /**
     * @brief 获取当前命令缓冲区
     */
    CommandBuffer *commandBuffer() const {
        return commandBuffer_;
    }

    /**
     * @brief 获取当前尺寸
     */
    void getSize(int *width, int *height) const;

private:
    CommandBuffer *commandBuffer_ = nullptr;
    int width_ = 0;
    int height_ = 0;

    struct State {
        float tx = 0.0f, ty = 0.0f;
        float sx = 1.0f, sy = 1.0f;
        float opacity = 1.0f;
    };
    std::vector<State> stateStack_;
    State currentState_;

    Rect transformRect(const Rect &rect) const;
    Color applyOpacity(const Color &color) const;

    /**
     * @brief 添加命令到缓冲区
     */
    void addCommand(Command cmd);

    /**
     * @brief 检查命令缓冲区是否有效
     */
    bool checkCommandBuffer() const;
};
