// PlatformWindowWayland 骨架实现——全部为占位（协议层未接线）。
// 语义约定：Create 失败返回 false，调用方（Application）得到明确错误日志
// 而非崩溃；其余 no-op 保证链接完整。接线路线见 .cppm 头注释。
module;

#include <cstdint>

module kwik.platform.wayland_window;

import kwik.core.log;
import std;

PlatformWindowWayland::PlatformWindowWayland() = default;
PlatformWindowWayland::~PlatformWindowWayland() = default;

bool PlatformWindowWayland::Create(const std::string &title, int width, int height) {
    (void)title;
    (void)width;
    (void)height;
    Log::error("PlatformWindowWayland::Create: wayland 协议层未接线（骨架占位），"
               "win32 后端可用，wayland 落地见任务清单 T4-61");
    return false;
}

void PlatformWindowWayland::Destroy() {}

void PlatformWindowWayland::Show() {}
void PlatformWindowWayland::Hide() {}

void PlatformWindowWayland::GetSize(int *width, int *height) const {
    if (width) *width = width_;
    if (height) *height = height_;
}

// 骨架期固定 1.0；落地时经 wl_output scale 事件 + fractional-scale 协议取真值
float PlatformWindowWayland::GetDpiScale() const { return 1.0f; }

void PlatformWindowWayland::GetScreenWorkArea(int *width, int *height) {
    if (width) *width = 0;
    if (height) *height = 0;
}

bool PlatformWindowWayland::LockBackBuffer(void **pixels, int *stride) {
    (void)pixels;
    (void)stride;
    return false;    // 软件路径 = wl_shm 缓冲，待接
}

void PlatformWindowWayland::UnlockBackBuffer() {}
void PlatformWindowWayland::Present() {}

void PlatformWindowWayland::PollEvents() {}    // wl_display_dispatch_pending，待接
void PlatformWindowWayland::WaitEvents() {}    // wl_display 阻塞读，待接

void PlatformWindowWayland::SetRawEventCallback(RawEventCallback callback) {
    // 唯一先接线的点：回调先存住，wl_seat 接线后直接向它产事件
    rawCallback_ = std::move(callback);
}

void PlatformWindowWayland::SetDecoration(WindowDecoration decoration) {
    decoration_ = decoration;    // xdg_toplevel 装饰（server-side / CSD），待接
}

void PlatformWindowWayland::SetShape(const std::vector<std::pair<int, int>> &polygon) {
    (void)polygon;    // wayland 无任意多边形形状；可经 wl_region 近似，暂不支持
}

void PlatformWindowWayland::SetShapeMask(const uint8_t *maskData, int width, int height) {
    (void)maskData;
    (void)width;
    (void)height;
}

void PlatformWindowWayland::SetResizable(bool resizable) {
    (void)resizable;    // xdg_toplevel set_resizable，待接
}
