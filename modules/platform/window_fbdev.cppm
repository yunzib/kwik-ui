module;

#include <cstdint>

export module kwik.platform.fbdev_window;

import kwik.platform.window;
import std;

/**
 * @brief Linux Framebuffer（/dev/fb*）平台窗口实现（骨架占位）
 *
 * 需求边界（2026-10-07 用户定案）：本后端只覆盖 **有 GPU + Vulkan** 的场景，
 * 软件模拟不做——LockBackBuffer 恒返回 false（纯设计语义，非"待接"），
 * 呈现仅走 GPU 路径。有 DRM 设备节点的场景请优先用 drm 后端（KMS/GBM）。
 *
 * 定位：面向仅暴露 /dev/fb*（无 /dev/dri 或不希望接管 KMS）的嵌入式设备。
 *
 * 落地路径（待接，对应任务清单 T4-61）：
 * - 窗口：open("/dev/fb0") → FBIOGET_VSCREENINFO/FIXEDSCREENINFO 取当前
 *   mode（width/height/bpp/stride；全屏接管，无标题栏/形状语义）
 * - 呈现：Vulkan **VK_KHR_display**（直接到显示器，无需合成器/原生句柄）
 *   或离屏渲染后 GBM/drm blit——GetNativeHandle 返回 nullptr（VK_KHR_display
 *   不需要原生句柄；若走 GBM 互操作则改返回 gbm_device）
 * - 输入：libevdev 读 /dev/input/event*（与 drm 后端同款，可共享翻译层）
 * - DPI：固定 1.0（物理像素直出）
 */
export class PlatformWindowFBDev : public PlatformWindow {
public:
    PlatformWindowFBDev();
    ~PlatformWindowFBDev() override;

    // 窗口生命周期
    bool Create(const std::string &title, int width, int height) override;
    void Destroy() override;
    void Show() override;
    void Hide() override;
    void GetSize(int *width, int *height) const override;
    float GetDpiScale() const override;
    void GetScreenWorkArea(int *width, int *height) override;
    // 软件渲染——按需求边界恒不支持（本后端仅 GPU/Vulkan 呈现）
    bool LockBackBuffer(void **pixels, int *stride) override;
    void UnlockBackBuffer() override;
    void Present() override;
    // GPU 渲染
    void *GetNativeHandle() const override { return nativeHandle_; }
    // 事件
    void PollEvents() override;
    void WaitEvents() override;
    void SetRawEventCallback(RawEventCallback callback) override;
    // 窗口定制（全屏接管语义，恒 no-op）
    void SetDecoration(WindowDecoration decoration) override;
    void SetShape(const std::vector<std::pair<int, int>> &polygon) override;
    void SetShapeMask(const uint8_t *maskData, int width, int height) override;
    void SetResizable(bool resizable) override;

private:
    void *nativeHandle_ = nullptr;    // VK_KHR_display 无需原生句柄；走 GBM 互操作时存放 gbm_device*
    int fbFd_ = -1;                   // /dev/fb* 文件描述符（待接）
    int width_ = 0;
    int height_ = 0;
    bool created_ = false;
    RawEventCallback rawCallback_ = nullptr;
};
