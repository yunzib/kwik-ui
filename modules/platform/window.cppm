export module kwik.platform.window;
import std;
import kwik.event;
import kwik.core.types;    // DragRegion / WindowHitZone（无边框命中测试纯几何）

/**
 * @brief 事件类型枚举
 */
export struct Event {
    enum class Type {
        None,            // 无事件
        MouseMove,       // 鼠标移动
        MouseDown,       // 鼠标按下
        MouseUp,         // 鼠标释放
        MouseWheel,      // 鼠标滚轮
        KeyDown,         // 键盘按下
        KeyUp,           // 键盘释放
        TouchBegin,      // 触摸开始
        TouchMove,       // 触摸移动
        TouchEnd,        // 触摸结束
        TouchCancel,     // 触摸取消
        WindowResize,    // 窗口大小改变
        WindowClose,     // 窗口关闭
        WindowPaint,     // 窗口绘制
        TextInput,       // 字符输入 (含 IME 组合结果)
    } type = Type::None;
    // 鼠标按钮枚举
    enum class MouseButton { None, Left, Right, Middle } button = MouseButton::None;
    int x = 0;                  // X坐标（客户区）
    int y = 0;                  // Y坐标（客户区）
    int width = 0;              // 窗口宽度
    int height = 0;             // 窗口高度
    std::uint32_t keyCode = 0;       // 虚拟键码 (VK_*), KeyDown/KeyUp 使用
    std::uint32_t modifiers = 0;     // Ctrl(1), Shift(2), Alt(4), Meta(8)
    std::uint32_t charCode = 0;      // Unicode 码点, TextInput 使用
    int touchId = 0;            // 触摸点ID
    float pressure = 1.0f;      // 触摸压力
    float wheelDelta = 0.0f;    // 滚轮增量
};

/**
 * @brief 窗口装饰风格
 */
export enum class WindowDecoration {
    Normal,        // 默认带标题栏和边框
    Borderless,    // 无边框（保留原生缩放边缘 / Aero Snap / 阴影；拖拽区由 SetDragRegions 声明）
    Transparent    // 透明背景（用于异形窗口）
};

/** 基准屏参照系（全库唯一定义）：px=逻辑像素模型中
 *  S₀ = 所在屏工作区 ÷ 基准屏；1920×1080 为约定基准，非设备假设 */
export inline constexpr int kBaselineScreenW = 1920;
export inline constexpr int kBaselineScreenH = 1080;

/**
 * @brief 窗口创建选项
 *
 * 默认值 = 旧 Create(title, w, h) 行为（Normal / 可缩放 / 无图标 / 进任务栏）。
 * 各平台按能力接线，未接线平台走默认 Create 重载保持行为不变。
 */
export struct WindowOptions {
    WindowDecoration decoration = WindowDecoration::Normal;    ///< 装饰风格
    bool resizable = true;                                     ///< 是否允许缩放（无边框下仍保留原生边缘）
    std::string iconPath;                                      ///< 窗口/任务栏图标路径（.ico；空 = 系统默认）
    bool showInTaskbar = true;                                 ///< 是否出现在任务栏
};

/**
 * @brief 平台窗口抽象接口
 *
 * 提供跨平台的窗口创建、事件处理和渲染接口
 */
export class PlatformWindow {
public:
    using RawEventCallback = std::function<void(const RawEvent &)>;
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
     * @brief 创建窗口（带选项；默认实现忽略选项，等价旧 Create）
     *
     * 装饰/图标/任务栏可见性等初始化选项——需在窗口创建前生效的配置走这里；
     * 运行时变更走 SetDecoration / SetWindowIcon 等。
     */
    virtual bool Create(const std::string &title, int width, int height, const WindowOptions &options) {
        (void)options;
        return Create(title, width, height);
    }

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
     * @brief 获取窗口客户区逻辑尺寸（设计稿 px）
     *
     * 随窗口缩放动态变化的"当前设计稿尺寸"：物理客户区 ÷ 屏固有系数 S₀。
     * JS 布局坐标系与 getWindowSize 查询、onWindowResize 载荷均经此对齐
     */
    virtual void GetSize(int *width, int *height) const = 0;

    /**
     * @brief 获取当前 DPI 缩放比例 (物理像素 / 逻辑像素)
     * @return 96 DPI 时为 1.0, 192 DPI 时为 2.0
     */
    virtual float GetDpiScale() const = 0;

    /**
     * @brief 获取设计尺寸（Create 传入的原始逻辑尺寸）
     * 用于计算统一渲染系数 S；默认回退当前客户区尺寸（其他平台行为不变）
     */
    virtual void GetDesignSize(int *width, int *height) const { GetSize(width, height); }

    /**
     * @brief 获取主显示器工作区尺寸（不含任务栏），可在 Create() 前调用
     */
    virtual void GetScreenWorkArea(int *width, int *height) = 0;

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
     * @brief 轮询事件（非阻塞）
     */
    virtual void PollEvents() = 0;

    /**
     * @brief 等待事件（阻塞）
     */
    virtual void WaitEvents() = 0;
    // ==================== 窗口定制 ====================
    /**
     * @brief 设置事件回调函数
     */
    virtual void SetRawEventCallback(RawEventCallback callback) = 0;

    /**
     * @brief 模态循环精简泵注入点（win32 拖动/缩放专用）
     *
     * 系统拖动/缩放进入模态消息循环会吞掉应用主循环——tick 面全停
     * （Channel flush/定时器/rAF/动画冻结）。平台侧以 ~16ms 定时器在
     * 拖动模态循环内回收调本泵，驱动完整 tick（幂等：静止帧不
     * 渲染）。其余平台无模态循环语义，默认空实现不接线。
     */
    using ModalLoopPump = std::function<void()>;
    virtual void SetModalLoopPump(ModalLoopPump pump) {}

    /**
     * @brief 设置窗口装饰风格
     */
    virtual void SetDecoration(WindowDecoration decoration) = 0;

    /**
     * @brief 设置无边框拖拽区（逻辑像素、窗口坐标；命中区域按 HTCAPTION 处理）
     *
     * 需先切到 Borderless；空列表 = 清除。命中拖拽区 = 系统"点住标题栏"语义：
     * 按住拖动移动窗口、双击最大化/还原。HTCAPTION 会抢走点击——拖拽区不得
     * 覆盖需要 onClick 的控制按钮；窗口最外侧缩放带优先于拖拽区（边缘可缩放）。
     * 窗口 resize 后调用方需重新声明（坐标随窗口尺寸变化）。
     */
    virtual void SetDragRegions(const std::vector<DragRegion> &regions) { (void)regions; }

    // ── 窗口控制（平台无能力时 no-op；JS 命令面同源）──
    virtual void MinimizeWindow() {}
    virtual void MaximizeWindow() {}
    virtual void RestoreWindow() {}
    virtual bool IsWindowMaximized() const { return false; }
    /** @brief 请求关闭（等价点关闭按钮：走 WM_CLOSE 生命周期，不直接销毁） */
    virtual void CloseWindow() {}

    /** @brief 设置窗口/任务栏图标（.ico；返回是否成功） */
    virtual bool SetWindowIcon(const std::string &path) { (void)path; return false; }

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
    virtual void SetShapeMask(const std::uint8_t *maskData, int width, int height) = 0;

    /**
     * @brief 设置窗口是否可调整大小
     */
    virtual void SetResizable(bool resizable) = 0;

    /**
     * @brief 读取剪贴板文本（UTF-8）
     * @return 成功返回 true
     */
    virtual bool GetClipboardText(std::string &out) { return false; }

    /**
     * @brief 写入剪贴板文本（UTF-8）
     */
    virtual void SetClipboardText(const std::string &text) {}

    /**
     * @brief 读取剪贴板自定义格式数据（应用注册格式，字节序列）
     * @param fmtName 格式注册名（如 "kwik/text-runs-v1"）
     * @return false = 平台不支持/格式不存在/打开失败
     */
    virtual bool GetClipboardCustom(const char *fmtName, std::string &out) { return false; }

    /**
     * @brief 向剪贴板追加自定义格式数据（不清空已有格式——需先 SetClipboardText）
     * @return false = 平台不支持/打开失败/分配失败
     */
    virtual bool SetClipboardCustom(const char *fmtName, const std::string &bytes) { return false; }
};
