// PlatformWindowHarmony 骨架实现——全部为占位（OHOS SDK/XComponent 未接线）。
// 窗口生命周期由 ArkUI Surface 回调驱动（OnSurfaceCreated/Changed/Destroyed
// 桥接进 Create/resize/Destroy），与桌面"调用方主动 Create"模型不同，
// 接线时以回调注入为主。呈现仅 GPU/Vulkan（VK_OHOS_surface），软件路径不做。
// 接线路线见 .cppm 头注释。
module;

#include <cstdint>

module kwik.platform.harmony_window;

import kwik.core.log;
import std;

PlatformWindowHarmony::PlatformWindowHarmony() = default;

PlatformWindowHarmony::~PlatformWindowHarmony() { Destroy(); }

bool PlatformWindowHarmony::Create(const std::string &title, int width, int height) {
    (void)title;
    (void)width;
    (void)height;
    Log::error("PlatformWindowHarmony::Create: OHOS SDK 未接线（骨架占位），"
               "落地见任务清单 T4-61");
    return false;
}

void PlatformWindowHarmony::Destroy() {}    // 待接：OH_NativeWindow 引用释放

void PlatformWindowHarmony::Show() {
    visible_ = true;    // 待接：由页面可见性回调驱动
}

void PlatformWindowHarmony::Hide() {
    visible_ = false;    // 待接：由页面可见性回调驱动
}

void PlatformWindowHarmony::GetSize(int *width, int *height) const {
    if (width) *width = width_;
    if (height) *height = height_;
}

// 待接：vp→px 密度因子经 NDK 取（与 win32 GetDpiScale 物理逻辑比同口径）
float PlatformWindowHarmony::GetDpiScale() const { return 1.0f; }

void PlatformWindowHarmony::GetScreenWorkArea(int *width, int *height) {
    // 全屏 XComponent：工作区 = 窗口自身（避让区由系统处理）
    if (width) *width = width_;
    if (height) *height = height_;
}

bool PlatformWindowHarmony::LockBackBuffer(void **pixels, int *stride) {
    (void)pixels;
    (void)stride;
    return false;    // CPU 直写不可用——呈现仅 Vulkan（VK_OHOS_surface）
}

void PlatformWindowHarmony::UnlockBackBuffer() {}
void PlatformWindowHarmony::Present() {}    // 呈现由 Vulkan present 完成

void PlatformWindowHarmony::PollEvents() {}    // 待接：ArkUI 触摸事件回调桥接
void PlatformWindowHarmony::WaitEvents() {}    // 同上（事件由系统线程投递）

void PlatformWindowHarmony::SetRawEventCallback(RawEventCallback callback) {
    // 唯一先接线的点：回调先存住，XComponent 事件桥接后直接向它产事件
    rawCallback_ = std::move(callback);
}

void PlatformWindowHarmony::SetDecoration(WindowDecoration decoration) {
    (void)decoration;    // 系统栏/沉浸式由页面配置决定
}

void PlatformWindowHarmony::SetShape(const std::vector<std::pair<int, int>> &polygon) {
    (void)polygon;    // 不可表达
}

void PlatformWindowHarmony::SetShapeMask(const uint8_t *maskData, int width, int height) {
    (void)maskData;
    (void)width;
    (void)height;
}

void PlatformWindowHarmony::SetResizable(bool resizable) {
    (void)resizable;    // 窗口由系统全屏给定
}
