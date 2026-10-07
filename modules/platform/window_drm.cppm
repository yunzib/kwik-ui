module;

#include <cstdint>

export module kwik.platform.drm_window;

import kwik.platform.window;
import std;

/**
 * @brief DRM/KMS 平台窗口实现（骨架占位）——无合成器的裸 Linux 路径
 *
 * 现状：按 PlatformWindow 现接口占位——Create() 返回 false（KMS/GBM 未接线），
 * 不依赖 libdrm/GBM/EGL 头文件（CMake 依赖探测已降为可选），全平台可编译。
 *
 * 落地路径（待接，对应任务清单 T4-61）：
 * - 窗口：/dev/dri/cardN 打开 → drmModeGetResources → KMS connector/mode
 *   （全屏接管，无标题栏语义；SetDecoration/SetShape 为天然 no-op）
 * - 呈现：GBM surface + drmModeAtomicCommit 页翻转；GPU 路径走 Vulkan
 *   （VK_EXT_physical_device_drm 枚举设备，GetNativeHandle 返回 GBM 表面），
 *   无 GPU 场景走软件光栅 + LockBackBuffer（drmModeMapFB/dumb buffer）
 * - 输入：libevdev 读 /dev/input/event*（无组合器，无 IME 系统输入——
 *   文本输入依赖软键盘注入，同 win32 的 keyboard.cpp 注入通路）
 * - DPI：固定 1.0（物理像素直出）直至引入 per-connector 缩放策略
 */
export class PlatformWindowDRM : public PlatformWindow {
public:
    PlatformWindowDRM();
    ~PlatformWindowDRM() override;

    // 窗口生命周期
    bool Create(const std::string &title, int width, int height) override;
    void Destroy() override;
    void Show() override;
    void Hide() override;
    void GetSize(int *width, int *height) const override;
    float GetDpiScale() const override;
    void GetScreenWorkArea(int *width, int *height) override;
    // 软件渲染
    bool LockBackBuffer(void **pixels, int *stride) override;
    void UnlockBackBuffer() override;
    void Present() override;
    // GPU 渲染
    void *GetNativeHandle() const override { return gbmSurface_; }
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
    void *gbmSurface_ = nullptr;    // struct gbm_surface*（待接）
    void *drmFd_ = nullptr;         // DRM 设备文件描述符语境（待接；int fd 存此）
    int width_ = 0;
    int height_ = 0;
    bool created_ = false;          // KMS 接管中（Hide/Show 语义依赖）
    RawEventCallback rawCallback_ = nullptr;
};
