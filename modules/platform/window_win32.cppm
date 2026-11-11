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
    bool Create(const std::string &title, int width, int height, const WindowOptions &options) override;
    void Destroy() override;
    void Show() override;
    void Hide() override;
    void GetSize(int *width, int *height) const override;
    float GetDpiScale() const override;
    void RefitToNearestMonitor();
    void GetDesignSize(int *width, int *height) const override;
    void GetScreenWorkArea(int *width, int *height) override;
    // 软件渲染
    bool LockBackBuffer(void **pixels, int *stride) override;
    void UnlockBackBuffer() override;
    void Present() override;
    // GPU渲染
    void *GetNativeHandle() const override { return hwnd_; }
    void PollEvents() override;
    void WaitEvents() override;
    // 窗口定制
    void SetDecoration(WindowDecoration decoration) override;
    void SetDragRegions(const std::vector<DragRegion> &regions) override;
    void MinimizeWindow() override;
    void MaximizeWindow() override;
    void RestoreWindow() override;
    bool IsWindowMaximized() const override;
    void CloseWindow() override;
    bool SetWindowIcon(const std::string &path) override;
    void SetShape(const std::vector<std::pair<int, int>> &polygon) override;
    void SetShapeMask(const uint8_t *maskData, int width, int height) override;
    void SetResizable(bool resizable) override;

    void SetRawEventCallback(PlatformWindow::RawEventCallback callback) override;

    void SetModalLoopPump(PlatformWindow::ModalLoopPump pump) override { modalPump_ = std::move(pump); }

    // 剪贴板
    bool GetClipboardText(std::string &out) override;
    void SetClipboardText(const std::string &text) override;
    bool GetClipboardCustom(const char *fmtName, std::string &out) override;
    bool SetClipboardCustom(const char *fmtName, const std::string &bytes) override;

private:
    // 窗口消息处理
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    /// 无边框命中测试（WM_NCHITTEST；拖拽区逻辑坐标按当前屏 S₀ 换算物理）
    LRESULT HandleFramelessHitTest(LPARAM lParam);
    /// 加载 .ico 并应用为窗口/类图标（失败返回 false，保留旧图标）
    bool ApplyIconPath(const std::string &path);
    /// 当前显示器 S₀（工作区 ÷ 基准屏，与 GetSize 同源）
    float CurrentS0() const;
    HWND hwnd_ = nullptr;           // 窗口句柄
    HDC hdc_ = nullptr;             // 设备上下文
    HBITMAP bitmap_ = nullptr;      // 位图缓冲区
    void *bitmapBits_ = nullptr;    // 位图像素数据
    int width_ = 0;                 // 窗口宽度
    int height_ = 0;                // 窗口高度
    PlatformWindow::ModalLoopPump modalPump_;    // 拖动/缩放模态循环精简泵
    int designWidth_ = 800;         // Create() 传入的原始逻辑宽度（缩放前）
    int designHeight_ = 600;        // Create() 传入的原始逻辑高度（缩放前）
    WindowDecoration decoration_ = WindowDecoration::Normal;
    std::vector<DragRegion> dragRegions_;    // 无边框拖拽区（逻辑像素、窗口坐标）
    HICON hIconLarge_ = nullptr;             // 窗口/任务栏大图标（WM_SETICON + 类图标）
    HICON hIconSmall_ = nullptr;             // 标题栏/Alt-Tab 小图标
    uint8_t windowState_ = 0;                // 0=普通 1=最大化 2=最小化（变化时发 WindowStateChanged）
    RawEventCallback rawCallback_ = nullptr;
    float grabRelX_ = 0.5f;   // 抓取点在窗口内的归一化位置（0~1）：ENTERSIZEMOVE 时记录，
    float grabRelY_ = 0.5f;   // 拖动全程钉住——跨屏尺寸变化后 WM_MOVE 持续纠偏的基准
    RECT enterRect_{};    // 进入模态循环时的窗口矩形（判别 移动 vs 手动缩放）
    HMONITOR enterMon_ = nullptr;    // 进入模态循环时所在屏（判别是否跨屏）
    HMONITOR moveTrackMon_ = nullptr;    // 拖动中最近一次所在屏（WM_MOVE 更新）
    bool moveCrossed_ = false;           // 本轮拖动曾跨越屏幕边界
    HMONITOR refitMon_ = nullptr;        // 最近一次 Refit 归属屏（WM_MOVE 翻转检测：跨屏即时重设窗口）
    bool inMoveLoop_ = false;            // 移动模态循环中（WM_SIZING 到达即撤销——缩放循环不纠偏）
    bool captured_ = false;              // 左键捕获持有中（区分自愿释放/被夺，防误发 Cancel）
    ULONGLONG lastSizingTick_ = 0;       // 上条 WM_SIZING 时刻（放大限速时间基准；0=本会话未初始化）
    int lastSizingW_ = 0;                // 上条 WM_SIZING 后的窗口尺寸（限速基准）
    int lastSizingH_ = 0;
};

#endif    // _WIN32