module;

#include <functional>
#include <string>
#include <vector>
#include <cstdint>

export module kwik.platform.window;

/**
 * @brief 事件类型枚举
 */
export struct Event {
    enum class Type {
        None,         // 无事件
        MouseMove,    // 鼠标移动
        MouseDown,    // 鼠标按下
        MouseUp,      // 鼠标释放
        MouseWheel,   // 鼠标滚轮
        KeyDown,      // 键盘按下
        KeyUp,        // 键盘释放
        TouchBegin,   // 触摸开始
        TouchMove,    // 触摸移动
        TouchEnd,     // 触摸结束
        TouchCancel,  // 触摸取消
        WindowResize, // 窗口大小改变
        WindowClose,  // 窗口关闭
        WindowPaint   // 窗口绘制
    } type = Type::None;
    // 鼠标按钮枚举
    enum class MouseButton { None, Left, Right, Middle } button = MouseButton::None;
    int x = 0;               // X坐标（客户区）
    int y = 0;               // Y坐标（客户区）
    int width = 0;           // 窗口宽度
    int height = 0;          // 窗口高度
    uint32_t keyCode = 0;    // 平台无关键码
    uint32_t modifiers = 0;  // 修饰键：Ctrl(1), Shift(2), Alt(4), Meta(8)
    int touchId = 0;         // 触摸点ID
    float pressure = 1.0f;   // 触摸压力
    float wheelDelta = 0.0f; // 滚轮增量
};

/**
 * @brief 窗口装饰风格
 */
export enum class WindowDecoration {
    Normal,     // 默认带标题栏和边框
    Borderless, // 无边框
    Transparent // 透明背景（用于异形窗口）
};

/**
 * @brief 平台窗口抽象接口
 *
 * 提供跨平台的窗口创建、事件处理和渲染接口
 */
export class PlatformWindow {
public:
    using EventCallback = std::function<void(const Event &)>;
    virtual ~PlatformWindow() = default;
    // ==================== 窗口生命周期 ====================

    /**
     * @brief 创建窗口
     * @param title 窗口标题
     * @param width 窗口宽度
     * @param height 窗口高度
     * @return 创建成功返回true
     */
    virtual bool Create(const std::string &title, int width, int height) = 0;

    /**
     * @brief 销毁窗口
     */
    virtual void Destroy() = 0;

    /**
     * @brief 显示窗口
     */
    virtual void Show() = 0;

    /**
     * @brief 隐藏窗口
     */
    virtual void Hide() = 0;

    /**
     * @brief 获取窗口大小
     */
    virtual void GetSize(int *width, int *height) const = 0;

    /**
     * @brief 获取当前 DPI 缩放比例 (物理像素 / 逻辑像素)
     * @return 96 DPI 时为 1.0, 192 DPI 时为 2.0
     */
    virtual float GetDpiScale() const = 0;

    // ==================== 渲染接口（软件方式） ====================

    /**
     * @brief 锁定后缓冲区用于软件渲染
     * @param pixels 输出像素缓冲区指针
     * @param stride 输出每行字节数
     * @return 成功返回true
     */
    virtual bool LockBackBuffer(void **pixels, int *stride) = 0;

    /**
     * @brief 解锁后缓冲区
     */
    virtual void UnlockBackBuffer() = 0;

    /**
     * @brief 呈现缓冲区内容到屏幕
     */
    virtual void Present() = 0;
    // ==================== 渲染接口（GPU方式） ====================

    /**
     * @brief 获取原生窗口句柄
     * @return Windows返回HWND，Linux返回wl_surface*或EGLSurface
     */
    virtual void *GetNativeHandle() const = 0;
    // ==================== 事件接口 ====================

    /**
     * @brief 设置事件回调函数
     */
    virtual void SetEventCallback(EventCallback callback) = 0;

    /**
     * @brief 轮询事件（非阻塞）
     */
    virtual void PollEvents() = 0;

    /**
     * @brief 等待事件（阻塞）
     */
    virtual void WaitEvents() = 0;
    // ==================== 窗口定制 ====================

    /**
     * @brief 设置窗口装饰风格
     */
    virtual void SetDecoration(WindowDecoration decoration) = 0;

    /**
     * @brief 设置窗口形状（通过多边形区域）
     * @param polygon 多边形顶点列表，空列表表示恢复矩形
     */
    virtual void SetShape(const std::vector<std::pair<int, int>> &polygon) = 0;

    /**
     * @brief 设置窗口形状（通过位图遮罩）
     * @param maskData 灰度图数据，0=透明，255=不透明
     * @param width 遮罩宽度
     * @param height 遮罩高度
     */
    virtual void SetShapeMask(const uint8_t *maskData, int width, int height) = 0;

    /**
     * @brief 设置窗口是否可调整大小
     */
    virtual void SetResizable(bool resizable) = 0;
};
