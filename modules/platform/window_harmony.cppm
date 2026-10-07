module;

#include <cstdint>

export module kwik.platform.harmony_window;

import kwik.platform.window;
import std;

/**
 * @brief HarmonyOS / OpenHarmony 平台窗口实现（骨架占位）
 *
 * 现状：按 PlatformWindow 现接口占位——Create() 返回 false（OHOS SDK
 * 未接线），不依赖 OHOS 头文件，全平台可编译。
 *
 * 落地路径（待接，对应任务清单 T4-61）：
 * - 窗口：ArkUI XComponent 挂载回调拿到 OHNativeWindow*
 *   （OH_NativeWindow_NativeXComponent_GetNativeWindow），生命周期
 *   OnSurfaceCreated/Changed/Destroyed 桥接 Create/resize/Destroy——
 *   Create 的 title/width/height 被忽略（窗口由 Ability/页面全屏给定）
 * - 呈现：Vulkan **VK_OHOS_surface**（OHNativeWindow* 创建 surface，
 *   GetNativeHandle 返回 OHNativeWindow*）；仅 GPU 路径
 * - 输入：ArkUI 触摸事件经 XComponent 回调 → RawEvent 翻译；文本输入经
 *   OH_Input/输入法框架（IME 负载契约 = UTF-8 整串）
 * - DPI：vp→px 密度因子经 NDK 取（与 win32 GetDpiScale 物理逻辑比同口径）
 */
export class PlatformWindowHarmony : public PlatformWindow {
public:
    PlatformWindowHarmony();
    ~PlatformWindowHarmony() override;

    // 窗口生命周期
    bool Create(const std::string &title, int width, int height) override;
    void Destroy() override;
    void Show() override;
    void Hide() override;
    void GetSize(int *width, int *height) const override;
    float GetDpiScale() const override;
    void GetScreenWorkArea(int *width, int *height) override;
    // 软件渲染（呈现由合成器管理，仅 Vulkan 路径有效）
    bool LockBackBuffer(void **pixels, int *stride) override;
    void UnlockBackBuffer() override;
    void Present() override;
    // GPU 渲染
    void *GetNativeHandle() const override { return nativeWindow_; }
    // 事件
    void PollEvents() override;
    void WaitEvents() override;
    void SetRawEventCallback(RawEventCallback callback) override;
    // 窗口定制（由系统 UI 决定，恒 no-op）
    void SetDecoration(WindowDecoration decoration) override;
    void SetShape(const std::vector<std::pair<int, int>> &polygon) override;
    void SetShapeMask(const uint8_t *maskData, int width, int height) override;
    void SetResizable(bool resizable) override;

private:
    void *nativeWindow_ = nullptr;    // OHNativeWindow*（待接，来自 XComponent）
    int width_ = 0;
    int height_ = 0;
    bool visible_ = false;
    RawEventCallback rawCallback_ = nullptr;
};
