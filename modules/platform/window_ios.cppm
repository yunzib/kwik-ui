module;

#include <cstdint>

export module kwik.platform.ios_window;

import kwik.platform.window;
import std;

/**
 * @brief iOS 平台窗口实现（骨架占位）
 *
 * 现状：按 PlatformWindow 现接口占位——Create() 返回 false（UIKit 未
 * 接线），不依赖 UIKit 头文件，全平台可编译。真实实现需要 Objective-C++
 * （.mm 翻译单元），接线时由 CMake 引入。
 *
 * 落地路径（待接，对应任务清单 T4-61）：
 * - 窗口：UIWindow/root UIViewController（AppDelegate 生命周期
 *   applicationDidBecomeActive 等桥接 Show/Hide）；Create 的 title/
 *   width/height 被忽略（窗口由设备全屏给定）
 * - 呈现：Vulkan 经 **MoltenVK**——VK_MVK_ios_surface，GetNativeHandle
   返回 UIView*（CAMetalLayer 载体）；仅 GPU 路径
 * - 输入：UITouch 系列 → RawEvent TouchBegin/Move/End/Cancel（天然映射，
 *   与 win32 触摸路径同款）；文本输入经系统软键盘（IME 负载契约 = UTF-8 整串）
 * - DPI：UIScreen scale（Retina 2.0/3.0）
 */
export class PlatformWindowIOS : public PlatformWindow {
public:
    PlatformWindowIOS();
    ~PlatformWindowIOS() override;

    // 窗口生命周期
    bool Create(const std::string &title, int width, int height) override;
    void Destroy() override;
    void Show() override;
    void Hide() override;
    void GetSize(int *width, int *height) const override;
    float GetDpiScale() const override;
    void GetScreenWorkArea(int *width, int *height) override;
    // 软件渲染（iOS 仅 GPU/Vulkan 呈现）
    bool LockBackBuffer(void **pixels, int *stride) override;
    void UnlockBackBuffer() override;
    void Present() override;
    // GPU 渲染
    void *GetNativeHandle() const override { return uiView_; }
    // 事件
    void PollEvents() override;
    void WaitEvents() override;
    void SetRawEventCallback(RawEventCallback callback) override;
    // 窗口定制（由系统全屏给定，恒 no-op）
    void SetDecoration(WindowDecoration decoration) override;
    void SetShape(const std::vector<std::pair<int, int>> &polygon) override;
    void SetShapeMask(const uint8_t *maskData, int width, int height) override;
    void SetResizable(bool resizable) override;

private:
    void *uiWindow_ = nullptr;    // UIWindow*（待接）
    void *uiView_ = nullptr;      // UIView*（待接，Vulkan surface 载体）
    int width_ = 0;
    int height_ = 0;
    bool visible_ = false;
    RawEventCallback rawCallback_ = nullptr;
};
