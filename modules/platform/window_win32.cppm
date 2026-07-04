module;

#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>

export module kwik.platform.win32_window;

import kwik.platform.window;
import kwik.core.types;
import std;

/**
 * @brief Windows平台窗口实现
 */
export class PlatformWindowWin32 : public PlatformWindow {
public:
    PlatformWindowWin32();
    ~PlatformWindowWin32() override;
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
    // GPU渲染
    void *GetNativeHandle() const override {
        return hwnd_;
    }
    void PollEvents() override;
    void WaitEvents() override;
    // 窗口定制
    void SetDecoration(WindowDecoration decoration) override;
    void SetShape(const std::vector<std::pair<int, int>> &polygon) override;
    void SetShapeMask(const uint8_t *maskData, int width, int height) override;
    void SetResizable(bool resizable) override;

    void SetRawEventCallback(PlatformWindow::RawEventCallback callback) override;

private:
    // 窗口消息处理
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    HWND hwnd_ = nullptr;        // 窗口句柄
    HDC hdc_ = nullptr;          // 设备上下文
    HBITMAP bitmap_ = nullptr;   // 位图缓冲区
    void *bitmapBits_ = nullptr; // 位图像素数据
    int width_ = 0;              // 窗口宽度
    int height_ = 0;             // 窗口高度
    int designWidth_ = 800;   // Create() 传入的原始逻辑宽度（缩放前）
    int designHeight_ = 600;  // Create() 传入的原始逻辑高度（缩放前）
    WindowDecoration decoration_ = WindowDecoration::Normal;
    RawEventCallback rawCallback_ = nullptr;
};

#endif // _WIN32