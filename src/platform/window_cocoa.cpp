// PlatformWindowCocoa 骨架实现——全部为占位（AppKit 未接线）。
// 真实实现需 Objective-C++（.mm）翻译单元处理 NSWindow/NSEvent/CAMetalLayer，
// 接线时由 CMake 按 APPLE 平台引入。呈现仅 GPU/Vulkan（MoltenVK
// VK_EXT_metal_surface），软件路径不做。接线路线见 .cppm 头注释。
module;

#include <cstdint>

module kwik.platform.cocoa_window;

import kwik.core.log;
import std;

PlatformWindowCocoa::PlatformWindowCocoa() = default;

PlatformWindowCocoa::~PlatformWindowCocoa() { Destroy(); }

bool PlatformWindowCocoa::Create(const std::string &title, int width, int height) {
    (void)title;
    (void)width;
    (void)height;
    Log::error("PlatformWindowCocoa::Create: AppKit 未接线（骨架占位），"
               "落地见任务清单 T4-61");
    return false;
}

void PlatformWindowCocoa::Destroy() {}    // 待接：NSWindow close + release（主线程）

void PlatformWindowCocoa::Show() {}       // 待接：[NSWindow makeKeyAndOrderFront:]
void PlatformWindowCocoa::Hide() {}       // 待接：[NSWindow orderOut:]

void PlatformWindowCocoa::GetSize(int *width, int *height) const {
    if (width) *width = width_;
    if (height) *height = height_;
}

// 待接：NSWindow backingScaleFactor（Retina 2.0）
float PlatformWindowCocoa::GetDpiScale() const { return 1.0f; }

void PlatformWindowCocoa::GetScreenWorkArea(int *width, int *height) {
    // 待接：NSScreen visibleFrame（不含菜单栏/Dock）
    if (width) *width = 0;
    if (height) *height = 0;
}

bool PlatformWindowCocoa::LockBackBuffer(void **pixels, int *stride) {
    (void)pixels;
    (void)stride;
    return false;    // 软件路径不做——仅 GPU/Vulkan（MoltenVK）
}

void PlatformWindowCocoa::UnlockBackBuffer() {}
void PlatformWindowCocoa::Present() {}    // 呈现由 Vulkan present（CAMetalLayer）完成

void PlatformWindowCocoa::PollEvents() {}    // 待接：NSApp sendEvent（主线程事件泵）
void PlatformWindowCocoa::WaitEvents() {}    // 待接：NSApp nextEventMatchingMask 阻塞

void PlatformWindowCocoa::SetRawEventCallback(RawEventCallback callback) {
    // 唯一先接线的点：回调先存住，NSEvent 接线后直接向它产事件
    rawCallback_ = std::move(callback);
}

void PlatformWindowCocoa::SetDecoration(WindowDecoration decoration) {
    decoration_ = decoration;    // 待接：NSWindowStyleMask（Borderless/Titled）
}

void PlatformWindowCocoa::SetShape(const std::vector<std::pair<int, int>> &polygon) {
    (void)polygon;    // 待接：NSWindow setOpaque:NO + 环形路径（低优）
}

void PlatformWindowCocoa::SetShapeMask(const uint8_t *maskData, int width, int height) {
    (void)maskData;
    (void)width;
    (void)height;
}

void PlatformWindowCocoa::SetResizable(bool resizable) {
    (void)resizable;    // 待接：NSWindowStyleMask resizable 位
}
