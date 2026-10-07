// PlatformWindowDRM 骨架实现——全部为占位（KMS/GBM 未接线）。
// DRM 是全屏接管语义：Show/Hide 即 DPMS 电源态，Present 即页翻转。
// 接线路线见 .cppm 头注释。
module;

#include <cstdint>

module kwik.platform.drm_window;

import kwik.core.log;
import std;

PlatformWindowDRM::PlatformWindowDRM() = default;
PlatformWindowDRM::~PlatformWindowDRM() { Destroy(); }

bool PlatformWindowDRM::Create(const std::string &title, int width, int height) {
    (void)title;
    (void)width;
    (void)height;
    Log::error("PlatformWindowDRM::Create: KMS/GBM 未接线（骨架占位），"
               "落地见任务清单 T4-61");
    return false;
}

void PlatformWindowDRM::Destroy() {
    // 待接：drmModeSetCrtc 还原、GBM surface 释放、fd 关闭
    created_ = false;
}

void PlatformWindowDRM::Show() {}     // 待接：DPMS On
void PlatformWindowDRM::Hide() {}     // 待接：DPMS Off

void PlatformWindowDRM::GetSize(int *width, int *height) const {
    if (width) *width = width_;
    if (height) *height = height_;
}

// 物理像素直出，无缩放策略；引入 per-connector 缩放时此处变更
float PlatformWindowDRM::GetDpiScale() const { return 1.0f; }

void PlatformWindowDRM::GetScreenWorkArea(int *width, int *height) {
    // 全屏接管：工作区 = 整个连接器当前 mode
    if (width) *width = width_;
    if (height) *height = height_;
}

bool PlatformWindowDRM::LockBackBuffer(void **pixels, int *stride) {
    (void)pixels;
    (void)stride;
    return false;    // dumb buffer 映射，待接（软件光栅路径）
}

void PlatformWindowDRM::UnlockBackBuffer() {}
void PlatformWindowDRM::Present() {}    // 待接：drmModeAtomicCommit 页翻转

void PlatformWindowDRM::PollEvents() {}    // 待接：libevdev 读 /dev/input/event*
void PlatformWindowDRM::WaitEvents() {}    // 待接：poll(2) 阻塞于 drm fd + evdev fd

void PlatformWindowDRM::SetRawEventCallback(RawEventCallback callback) {
    // 唯一先接线的点：回调先存住，evdev 接线后直接向它产事件
    rawCallback_ = std::move(callback);
}

void PlatformWindowDRM::SetDecoration(WindowDecoration decoration) {
    (void)decoration;    // 全屏接管无装饰语义，恒 no-op
}

void PlatformWindowDRM::SetShape(const std::vector<std::pair<int, int>> &polygon) {
    (void)polygon;    // 无合成器，形状窗口不可表达
}

void PlatformWindowDRM::SetShapeMask(const uint8_t *maskData, int width, int height) {
    (void)maskData;
    (void)width;
    (void)height;
}

void PlatformWindowDRM::SetResizable(bool resizable) {
    (void)resizable;    // 全屏接管恒可"调整"为当前 mode，无语义
}
