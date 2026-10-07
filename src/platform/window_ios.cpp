// PlatformWindowIOS 骨架实现——全部为占位（UIKit 未接线）。
// 真实实现需 Objective-C++（.mm）翻译单元处理 UIWindow/UITouch/CAMetalLayer，
// 接线时由 CMake 按 iOS 平台引入。呈现仅 GPU/Vulkan（MoltenVK
// VK_MVK_ios_surface），软件路径不做。接线路线见 .cppm 头注释。
module;

#include <cstdint>

module kwik.platform.ios_window;

import kwik.core.log;
import std;

PlatformWindowIOS::PlatformWindowIOS() = default;

PlatformWindowIOS::~PlatformWindowIOS() { Destroy(); }

bool PlatformWindowIOS::Create(const std::string &title, int width, int height) {
    (void)title;
    (void)width;
    (void)height;
    Log::error("PlatformWindowIOS::Create: UIKit 未接线（骨架占位），"
               "落地见任务清单 T4-61");
    return false;
}

void PlatformWindowIOS::Destroy() {}    // 待接：UIWindow 释放（主线程）

void PlatformWindowIOS::Show() {
    visible_ = true;    // 待接：由 applicationDidBecomeActive 驱动
}

void PlatformWindowIOS::Hide() {
    visible_ = false;    // 待接：由 applicationWillResignActive 驱动
}

void PlatformWindowIOS::GetSize(int *width, int *height) const {
    if (width) *width = width_;
    if (height) *height = height_;
}

// 待接：UIScreen scale（Retina 2.0/3.0）
float PlatformWindowIOS::GetDpiScale() const { return 1.0f; }

void PlatformWindowIOS::GetScreenWorkArea(int *width, int *height) {
    // iOS 全屏窗口：工作区 = 窗口自身（安全区由系统处理）
    if (width) *width = width_;
    if (height) *height = height_;
}

bool PlatformWindowIOS::LockBackBuffer(void **pixels, int *stride) {
    (void)pixels;
    (void)stride;
    return false;    // 软件路径不做——仅 GPU/Vulkan（MoltenVK）
}

void PlatformWindowIOS::UnlockBackBuffer() {}
void PlatformWindowIOS::Present() {}    // 呈现由 Vulkan present（CAMetalLayer）完成

void PlatformWindowIOS::PollEvents() {}    // 待接：UITouch 事件由系统主线程投递
void PlatformWindowIOS::WaitEvents() {}    // 同上（iOS 无自管事件泵）

void PlatformWindowIOS::SetRawEventCallback(RawEventCallback callback) {
    // 唯一先接线的点：回调先存住，UITouch 桥接后直接向它产事件
    rawCallback_ = std::move(callback);
}

void PlatformWindowIOS::SetDecoration(WindowDecoration decoration) {
    (void)decoration;    // 由系统全屏给定
}

void PlatformWindowIOS::SetShape(const std::vector<std::pair<int, int>> &polygon) {
    (void)polygon;    // 不可表达
}

void PlatformWindowIOS::SetShapeMask(const uint8_t *maskData, int width, int height) {
    (void)maskData;
    (void)width;
    (void)height;
}

void PlatformWindowIOS::SetResizable(bool resizable) {
    (void)resizable;    // iOS 窗口由系统全屏给定
}
