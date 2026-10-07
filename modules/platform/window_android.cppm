module;

#include <cstdint>

export module kwik.platform.android_window;

import kwik.platform.window;
import std;

/**
 * @brief Android 平台窗口实现（骨架占位）——NativeActivity 路径
 *
 * 现状：按 PlatformWindow 现接口占位——Create() 返回 false（NDK 未接线），
 * 不依赖 android/native_window 等头文件，全平台可编译。
 *
 * 落地路径（待接，对应任务清单 T4-61）：
 * - 窗口：android_native_app_glue 的 APP->window（ANativeWindow*），
 *   生命周期 onInitWindow/onTerminateWindow 桥接 Create/Destroy，
 *   APP_CMD_RESUME/PAUSE 桥接 Show/Hide——Create 的 title/width/height
 *   被忽略（Android 窗口由 Activity 全屏给定）
 * - 呈现：Vulkan VK_KHR_android_surface（GetNativeHandle 返回
 *   ANativeWindow*）；软件路径无意义（SurfaceView/TOP_OPAQUE 由合成器管）
 * - 输入：AInputQueue（AKeyEvent/A MotionEvent）→ RawEvent 翻译；
 *   文本输入经 JNI 调 InputMethodManager（软键盘），或 Keyboard 组件注入
 * - DPI：ANativeWindow_getWidth/Height ÷ AConfiguration 密度 → 与 win32
 *   GetDpiScale 同口径（物理/逻辑比）
 */
export class PlatformWindowAndroid : public PlatformWindow {
public:
    PlatformWindowAndroid();
    ~PlatformWindowAndroid() override;

    // 窗口生命周期
    bool Create(const std::string &title, int width, int height) override;
    void Destroy() override;
    void Show() override;
    void Hide() override;
    void GetSize(int *width, int *height) const override;
    float GetDpiScale() const override;
    void GetScreenWorkArea(int *width, int *height) override;
    // 软件渲染（Android 上不可用——合成器管理呈现，仅 Vulkan 路径有效）
    bool LockBackBuffer(void **pixels, int *stride) override;
    void UnlockBackBuffer() override;
    void Present() override;
    // GPU 渲染
    void *GetNativeHandle() const override { return nativeWindow_; }
    // 事件
    void PollEvents() override;
    void WaitEvents() override;
    void SetRawEventCallback(RawEventCallback callback) override;
    // 窗口定制
    void SetDecoration(WindowDecoration decoration) override;
    void SetShape(const std::vector<std::pair<int, int>> &polygon) override;
    void SetShapeMask(const uint8_t *maskData, int width, int height) override;
    void SetResizable(bool resizable) override;

private:
    void *nativeWindow_ = nullptr;    // ANativeWindow*（待接，来自 app_glue）
    int width_ = 0;
    int height_ = 0;
    bool visible_ = false;
    RawEventCallback rawCallback_ = nullptr;
};
