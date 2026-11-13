export module kwik.platform.wayland_window;

import kwik.platform.window;
import kwik.core.log;
import std;

/**
 * @brief Wayland 平台窗口实现（骨架占位）
 *
 * 现状：按 PlatformWindow 现接口占位——Create() 返回 false（协议层未接线），
 * 不依赖任何 wayland 头文件/库（CMake 依赖探测已降为可选），全平台可编译，
 * 作为 Linux 侧编译护栏与后续接线的落点。
 *
 * 落地路径（待接，对应任务清单 T4-61）：
 * - 窗口：wl_display/wl_registry → wl_compositor + wl_surface + xdg_shell
 *   （xdg_toplevel configure/ack_configure 承接 resize）
 * - 输入：wl_seat 的 wl_pointer/wl_keyboard → RawEvent；文本输入经
 *   zwp_text_input_v3（IME 负载契约 = UTF-8 整串，见任务清单 31/⑥）
 * - 呈现：Vulkan VK_KHR_wayland_surface（GetNativeHandle 返回 wl_surface*）
 *   或软件路径 LockBackBuffer（wl_shm 缓冲）
 */
export class PlatformWindowWayland : public PlatformWindow {
public:
    PlatformWindowWayland();
    ~PlatformWindowWayland() override;

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
    void *GetNativeHandle() const override { return surface_; }
    // 事件
    void PollEvents() override;
    void WaitEvents() override;
    void SetRawEventCallback(RawEventCallback callback) override;
    // 窗口定制
    void SetDecoration(WindowDecoration decoration) override;
    void SetShape(const std::vector<std::pair<int, int>> &polygon) override;
    void SetShapeMask(const std::uint8_t *maskData, int width, int height) override;
    void SetResizable(bool resizable) override;

private:
    void *surface_ = nullptr;    // wl_surface*（待接）
    int width_ = 0;
    int height_ = 0;
    WindowDecoration decoration_ = WindowDecoration::Normal;
    RawEventCallback rawCallback_ = nullptr;
};
