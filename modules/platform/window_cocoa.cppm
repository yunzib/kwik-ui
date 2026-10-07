module;

#include <cstdint>

export module kwik.platform.cocoa_window;

import kwik.platform.window;
import std;

/**
 * @brief macOS（Cocoa/AppKit）平台窗口实现（骨架占位）
 *
 * 现状：按 PlatformWindow 现接口占位——Create() 返回 false（AppKit 未
 * 接线），不依赖 Cocoa 头文件，全平台可编译。真实实现需要 Objective-C++
 * （.mm 翻译单元），接线时由 CMake 引入。
 *
 * 落地路径（待接，对应任务清单 T4-61）：
 * - 窗口：NSWindow/NSView（AppKit 主线程），resize 经
 *   windowDidResize: 代理回调；SetDecoration 映射 NSWindowStyleMask
 * - 呈现：Vulkan 经 **MoltenVK**——VK_EXT_metal_surface（CAMetalLayer），
 *   GetNativeHandle 返回 NSView*（surface 由其 layer 创建）；软件路径不做
 * - 输入：NSEvent（mouse/key）→ RawEvent 翻译；IME 经 NSTextInputClient
 *   （IME 负载契约 = UTF-8 整串）
 * - DPI：NSWindow backingScaleFactor（Retina 2.0）
 */
export class PlatformWindowCocoa : public PlatformWindow {
public:
    PlatformWindowCocoa();
    ~PlatformWindowCocoa() override;

    // 窗口生命周期
    bool Create(const std::string &title, int width, int height) override;
    void Destroy() override;
    void Show() override;
    void Hide() override;
    void GetSize(int *width, int *height) const override;
    float GetDpiScale() const override;
    void GetScreenWorkArea(int *width, int *height) override;
    // 软件渲染（macOS 仅 GPU/Vulkan 呈现）
    bool LockBackBuffer(void **pixels, int *stride) override;
    void UnlockBackBuffer() override;
    void Present() override;
    // GPU 渲染
    void *GetNativeHandle() const override { return nsView_; }
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
    void *nsWindow_ = nullptr;    // NSWindow*（待接）
    void *nsView_ = nullptr;      // NSView*（待接，Vulkan surface 载体）
    int width_ = 0;
    int height_ = 0;
    WindowDecoration decoration_ = WindowDecoration::Normal;
    RawEventCallback rawCallback_ = nullptr;
};
