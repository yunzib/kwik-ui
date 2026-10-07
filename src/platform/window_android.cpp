// PlatformWindowAndroid 骨架实现——全部为占位（NDK/app_glue 未接线）。
// Android 的窗口生命周期由 Activity 驱动（app_glue 回调桥接进 Create/Destroy/
// Show/Hide），与桌面平台的"调用方主动 Create"模型不同，接线时以回调注入为主。
// 接线路线见 .cppm 头注释。
module;

#include <cstdint>

module kwik.platform.android_window;

import kwik.core.log;
import std;

PlatformWindowAndroid::PlatformWindowAndroid() = default;

PlatformWindowAndroid::~PlatformWindowAndroid() { Destroy(); }

bool PlatformWindowAndroid::Create(const std::string &title, int width, int height) {
    (void)title;
    (void)width;
    (void)height;
    Log::error("PlatformWindowAndroid::Create: NDK 未接线（骨架占位），"
               "落地见任务清单 T4-61");
    return false;
}

void PlatformWindowAndroid::Destroy() {}    // 待接：ANativeWindow_release

void PlatformWindowAndroid::Show() {
    visible_ = true;    // 待接：由 APP_CMD_RESUME 驱动而非调用方
}

void PlatformWindowAndroid::Hide() {
    visible_ = false;    // 待接：由 APP_CMD_PAUSE 驱动
}

void PlatformWindowAndroid::GetSize(int *width, int *height) const {
    if (width) *width = width_;
    if (height) *height = height_;
}

// 待接：物理尺寸 ÷ AConfiguration 密度（ACONFIGURATION_DENSITY_*）与 win32 口径对齐
float PlatformWindowAndroid::GetDpiScale() const { return 1.0f; }

void PlatformWindowAndroid::GetScreenWorkArea(int *width, int *height) {
    // Android 全屏窗口：工作区 = 窗口自身（系统栏遮挡由合成器处理）
    if (width) *width = width_;
    if (height) *height = height_;
}

bool PlatformWindowAndroid::LockBackBuffer(void **pixels, int *stride) {
    (void)pixels;
    (void)stride;
    return false;    // CPU 直写 Surface 不可用——Android 呈现只走 GPU 路径
}

void PlatformWindowAndroid::UnlockBackBuffer() {}
void PlatformWindowAndroid::Present() {}    // 呈现由 eglSwapBuffers/Vulkan present 完成

void PlatformWindowAndroid::PollEvents() {}    // 待接：AInputQueue_getEvent 轮询
void PlatformWindowAndroid::WaitEvents() {}    // 待接：app_glue 主线程阻塞语义

void PlatformWindowAndroid::SetRawEventCallback(RawEventCallback callback) {
    // 唯一先接线的点：回调先存住，AInputQueue 接线后直接向它产事件
    rawCallback_ = std::move(callback);
}

void PlatformWindowAndroid::SetDecoration(WindowDecoration decoration) {
    (void)decoration;    // Activity 主题决定系统栏，运行时不可改
}

void PlatformWindowAndroid::SetShape(const std::vector<std::pair<int, int>> &polygon) {
    (void)polygon;    // 不可表达
}

void PlatformWindowAndroid::SetShapeMask(const uint8_t *maskData, int width, int height) {
    (void)maskData;
    (void)width;
    (void)height;
}

void PlatformWindowAndroid::SetResizable(bool resizable) {
    (void)resizable;    // Android 窗口由系统全屏给定，无语义
}
