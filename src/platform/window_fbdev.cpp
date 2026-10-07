// PlatformWindowFBDev 骨架实现——占位（fbdev/Vulkan 接线待接）。
// 需求边界：仅 GPU/Vulkan 呈现（VK_KHR_display 或 GBM blit），软件模拟
// 按定案不做——LockBackBuffer 恒 false。接线路线见 .cppm 头注释。
module;

#include <cstdint>

module kwik.platform.fbdev_window;

import kwik.core.log;
import std;

PlatformWindowFBDev::PlatformWindowFBDev() = default;

PlatformWindowFBDev::~PlatformWindowFBDev() { Destroy(); }

bool PlatformWindowFBDev::Create(const std::string &title, int width, int height) {
    (void)title;
    (void)width;
    (void)height;
    Log::error("PlatformWindowFBDev::Create: fbdev/Vulkan 未接线（骨架占位），"
               "落地见任务清单 T4-61");
    return false;
}

void PlatformWindowFBDev::Destroy() {
    // 待接：close(fbFd_)
    fbFd_ = -1;
    created_ = false;
}

void PlatformWindowFBDev::Show() {}     // 待接：FB_BLANK UNBLANK
void PlatformWindowFBDev::Hide() {}     // 待接：FB_BLANK POWERDOWN

void PlatformWindowFBDev::GetSize(int *width, int *height) const {
    if (width) *width = width_;
    if (height) *height = height_;
}

// 物理像素直出，无缩放策略
float PlatformWindowFBDev::GetDpiScale() const { return 1.0f; }

void PlatformWindowFBDev::GetScreenWorkArea(int *width, int *height) {
    // 全屏接管：工作区 = 当前 framebuffer mode
    if (width) *width = width_;
    if (height) *height = height_;
}

bool PlatformWindowFBDev::LockBackBuffer(void **pixels, int *stride) {
    (void)pixels;
    (void)stride;
    // 设计定案：软件模拟不做——本后端仅 GPU/Vulkan 呈现
    return false;
}

void PlatformWindowFBDev::UnlockBackBuffer() {}
void PlatformWindowFBDev::Present() {}    // 页翻转由 Vulkan present / KMS commit 完成

void PlatformWindowFBDev::PollEvents() {}    // 待接：libevdev 读 /dev/input/event*
void PlatformWindowFBDev::WaitEvents() {}    // 待接：poll(2) 阻塞于 fb fd + evdev fd

void PlatformWindowFBDev::SetRawEventCallback(RawEventCallback callback) {
    // 唯一先接线的点：回调先存住，evdev 接线后直接向它产事件
    rawCallback_ = std::move(callback);
}

void PlatformWindowFBDev::SetDecoration(WindowDecoration decoration) {
    (void)decoration;    // 全屏接管无装饰语义，恒 no-op
}

void PlatformWindowFBDev::SetShape(const std::vector<std::pair<int, int>> &polygon) {
    (void)polygon;    // 无合成器，形状窗口不可表达
}

void PlatformWindowFBDev::SetShapeMask(const uint8_t *maskData, int width, int height) {
    (void)maskData;
    (void)width;
    (void)height;
}

void PlatformWindowFBDev::SetResizable(bool resizable) {
    (void)resizable;    // 全屏接管，无语义
}
