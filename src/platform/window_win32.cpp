module;    // 全局模块片段开始

// DPI 感知: 高 DPI 屏上使用物理像素，避免坐标空间错位导致模糊
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE) - 4)
#endif

#include <windows.h>
#include <map>

module kwik.platform.win32_window;

import kwik.utils.string_utils;
import kwik.core.log;
import kwik.event;

// 全局窗口映射：HWND -> 对象指针
static std::map<HWND, PlatformWindowWin32 *> g_windowMap;

// ============================================================================
// 构造与析构
// ============================================================================
PlatformWindowWin32::PlatformWindowWin32() = default;
PlatformWindowWin32::~PlatformWindowWin32() {
    Destroy();
}

// ============================================================================
// 窗口生命周期
// ============================================================================
bool PlatformWindowWin32::Create(const std::string &title, int width, int height) {
    // DPI 感知
    {
        typedef BOOL(WINAPI * PfnSetProcessDpiAwarenessContext)(HANDLE);
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32) {
            auto pfn = (PfnSetProcessDpiAwarenessContext)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
            if (pfn) pfn((HANDLE)(-4));
        }
    }

    // 保存原始设计尺寸，供后续 DPI/屏幕切换时重新计算窗口大小
    designWidth_ = width;
    designHeight_ = height;

    // ── 设计分辨率模式：初始窗口 = 设计稿 × S₀
    //    S₀ = 主屏物理工作区 ÷ 基准屏(1920×1080)，与系统 DPI 设置完全无关；
    //    占屏比恒 ≈67%（1280/1920），纵横比由双向取小保证安全 ──
    RECT wa;
    int availW = kBaselineScreenW, availH = kBaselineScreenH;
    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0)) {
        availW = wa.right - wa.left;
        availH = wa.bottom - wa.top;
    }
    float s0w = (float)availW / kBaselineScreenW;
    float s0h = (float)availH / kBaselineScreenH;
    float S0 = s0w < s0h ? s0w : s0h;

    int scaledW = (int)(width * S0);
    int scaledH = (int)(height * S0);
    Log::info("[Fit] S0={} client={}x{} screen={}x{}", S0, scaledW, scaledH, GetSystemMetrics(SM_CXSCREEN),
              GetSystemMetrics(SM_CYSCREEN));

    // 物理钳制：设计×DPI 若超出主屏工作区则等比缩小，窗口永不超屏
    {
        RECT wa;
        if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0)) {
            int availW = (wa.right - wa.left) - 32;    // 余量含标题栏/边框
            int availH = (wa.bottom - wa.top) - 32;
            float sc = std::min(1.0f, std::min((float)availW / scaledW, (float)availH / scaledH));
            if (sc < 1.0f) {
                Log::info("[DPI] clamp x{} -> {}x{}", sc, (int)(scaledW * sc), (int)(scaledH * sc));
                scaledW = (int)(scaledW * sc);
                scaledH = (int)(scaledH * sc);
            }
        }
    }

    // 注册窗口类
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = L"KwikUIWindowClass";
    RegisterClassExW(&wc);
    // 窗口尺寸含装饰
    RECT rect = {0, 0, scaledW, scaledH};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    int windowWidth = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;
    // 居中
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenWidth - windowWidth) / 2;
    int posY = (screenHeight - windowHeight) / 2;
    std::wstring wtitle = Utf8ToWide(title);
    hwnd_ = CreateWindowExW(0, L"KwikUIWindowClass", wtitle.c_str(), WS_OVERLAPPEDWINDOW, posX, posY, windowWidth,
                            windowHeight, nullptr, nullptr, GetModuleHandle(nullptr), this);
    if (!hwnd_) return false;
    g_windowMap[hwnd_] = this;
    hdc_ = GetDC(hwnd_);
    width_ = scaledW;
    height_ = scaledH;
    return true;
}

void PlatformWindowWin32::Destroy() {
    // 释放位图缓冲区
    if (bitmap_) {
        DeleteObject(bitmap_);
        bitmap_ = nullptr;
        bitmapBits_ = nullptr;
    }

    // 释放设备上下文
    if (hdc_ && hwnd_) {
        ReleaseDC(hwnd_, hdc_);
        hdc_ = nullptr;
    }

    // 销毁窗口
    if (hwnd_) {
        DestroyWindow(hwnd_);
        g_windowMap.erase(hwnd_);
        hwnd_ = nullptr;
    }
}

void PlatformWindowWin32::Show() {
    if (hwnd_) { ShowWindow(hwnd_, SW_SHOW); }
}

void PlatformWindowWin32::Hide() {
    if (hwnd_) { ShowWindow(hwnd_, SW_HIDE); }
}

void PlatformWindowWin32::GetSize(int *width, int *height) const {
    if (width) *width = width_;
    if (height) *height = height_;
}

// ============================================================================
// 软件渲染接口
// ============================================================================
bool PlatformWindowWin32::LockBackBuffer(void **pixels, int *stride) {
    if (!hwnd_) return false;

    // 创建DIB位图（如果尚未创建）
    if (!bitmap_) {
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width_;
        bmi.bmiHeader.biHeight = -height_;    // 从上到下
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HDC screenDC = GetDC(nullptr);
        bitmap_ = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bitmapBits_, nullptr, 0);
        ReleaseDC(nullptr, screenDC);
    }

    if (pixels) *pixels = bitmapBits_;
    if (stride) *stride = width_ * 4;    // 32bpp

    return bitmap_ != nullptr;
}

void PlatformWindowWin32::UnlockBackBuffer() {
    // DIB不需要解锁操作
}

void PlatformWindowWin32::Present() {
    if (!hwnd_ || !bitmap_) return;

    // 将位图绘制到窗口
    HDC memDC = CreateCompatibleDC(hdc_);
    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memDC, bitmap_));
    BitBlt(hdc_, 0, 0, width_, height_, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBitmap);
    DeleteDC(memDC);
}

// ============================================================================
// 事件处理
// ============================================================================
void PlatformWindowWin32::PollEvents() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void PlatformWindowWin32::WaitEvents() {
    MSG msg;
    if (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

// ============================================================================
// 窗口定制
// ============================================================================
void PlatformWindowWin32::SetDecoration(WindowDecoration decoration) {
    if (!hwnd_) return;

    decoration_ = decoration;
    LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_STYLE);

    switch (decoration) {
    case WindowDecoration::Borderless:
        // 移除标题栏和边框
        style &= ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
        style |= WS_POPUP;
        break;
    case WindowDecoration::Transparent:
        // 透明窗口
        style &= ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME);
        style |= WS_POPUP;
        // 设置分层窗口
        SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, GetWindowLongPtrW(hwnd_, GWL_EXSTYLE) | WS_EX_LAYERED);
        break;
    case WindowDecoration::Normal:
    default:
        // 恢复默认样式
        style |= WS_OVERLAPPEDWINDOW;
        break;
    }

    SetWindowLongPtrW(hwnd_, GWL_STYLE, style);
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

void PlatformWindowWin32::SetShape(const std::vector<std::pair<int, int>> &polygon) {
    if (!hwnd_) return;

    if (polygon.empty()) {
        // 恢复矩形窗口
        SetWindowRgn(hwnd_, nullptr, TRUE);
        return;
    }

    // 将多边形转换为POINT数组
    std::vector<POINT> pts;
    pts.reserve(polygon.size());
    for (const auto &p : polygon) { pts.push_back({p.first, p.second}); }

    // 创建多边形区域
    HRGN rgn = CreatePolygonRgn(pts.data(), static_cast<int>(pts.size()), ALTERNATE);
    SetWindowRgn(hwnd_, rgn, TRUE);    // 窗口获得区域所有权
}

void PlatformWindowWin32::SetShapeMask(const uint8_t *maskData, int width, int height) {
    if (!hwnd_ || !maskData) return;

    // 从遮罩创建区域
    HRGN rgn = nullptr;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width;) {
            // 跳过透明区域
            if (maskData[y * width + x] == 0) {
                ++x;
                continue;
            }

            // 找到连续的不透明区域
            int startX = x;
            while (x < width && maskData[y * width + x] > 0) { ++x; }

            // 添加矩形到区域
            HRGN rectRgn = CreateRectRgn(startX, y, x, y + 1);
            if (!rgn) {
                rgn = rectRgn;
            } else {
                CombineRgn(rgn, rgn, rectRgn, RGN_OR);
                DeleteObject(rectRgn);
            }
        }
    }

    SetWindowRgn(hwnd_, rgn, TRUE);
}

void PlatformWindowWin32::SetResizable(bool resizable) {
    if (!hwnd_) return;

    LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_STYLE);

    if (resizable) {
        style |= WS_THICKFRAME;
    } else {
        style &= ~WS_THICKFRAME;
    }

    SetWindowLongPtrW(hwnd_, GWL_STYLE, style);
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

// ============================================================================
// 窗口消息处理
// ============================================================================
LRESULT CALLBACK PlatformWindowWin32::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto it = g_windowMap.find(hwnd);
    if (it != g_windowMap.end()) { return it->second->HandleMessage(msg, wParam, lParam); }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT PlatformWindowWin32::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!rawCallback_) { return DefWindowProcW(hwnd_, msg, wParam, lParam); }
    Event e;

    switch (msg) {
    // 鼠标移动
    case WM_MOUSEMOVE:
        e.type = Event::Type::MouseMove;
        e.x = static_cast<int>(LOWORD(lParam));
        e.y = static_cast<int>(HIWORD(lParam));
        if (wParam & MK_LBUTTON)
            e.button = Event::MouseButton::Left;
        else if (wParam & MK_RBUTTON)
            e.button = Event::MouseButton::Right;
        else if (wParam & MK_MBUTTON)
            e.button = Event::MouseButton::Middle;
        else
            e.button = Event::MouseButton::None;
        break;

    // 鼠标按键
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        e.type = (msg == WM_LBUTTONDOWN) ? Event::Type::MouseDown : Event::Type::MouseUp;
        e.button = Event::MouseButton::Left;
        e.x = static_cast<int>(LOWORD(lParam));
        e.y = static_cast<int>(HIWORD(lParam));
        break;

    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        e.type = (msg == WM_RBUTTONDOWN) ? Event::Type::MouseDown : Event::Type::MouseUp;
        e.button = Event::MouseButton::Right;
        e.x = static_cast<int>(LOWORD(lParam));
        e.y = static_cast<int>(HIWORD(lParam));
        break;

    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
        e.type = (msg == WM_MBUTTONDOWN) ? Event::Type::MouseDown : Event::Type::MouseUp;
        e.button = Event::MouseButton::Middle;
        e.x = static_cast<int>(LOWORD(lParam));
        e.y = static_cast<int>(HIWORD(lParam));
        break;

    // 鼠标滚轮
    // 鼠标滚轮 (lParam 为屏幕坐标, 需转客户区)
    // 鼠标滚轮 (lParam 为屏幕坐标, 需转客户区)
    case WM_MOUSEWHEEL: {
        e.type = Event::Type::MouseWheel;
        e.wheelDelta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
        POINT pt = {(int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam)};
        ScreenToClient(hwnd_, &pt);
        e.x = pt.x;
        e.y = pt.y;
        break;
    }

    // 字符输入 (含 IME 组合) — 分离 TextInput 和 KeyDown, 支持中文输入
    case WM_CHAR:
    case WM_IME_CHAR:
        e.type = Event::Type::TextInput;
        e.charCode = static_cast<uint32_t>(wParam);
        break;

    // 键盘事件
    case WM_KEYDOWN:
    case WM_KEYUP:
        e.type = (msg == WM_KEYDOWN) ? Event::Type::KeyDown : Event::Type::KeyUp;
        e.keyCode = static_cast<uint32_t>(wParam);
        e.modifiers = 0;
        if (GetKeyState(VK_CONTROL) & 0x8000) e.modifiers |= 1;
        if (GetKeyState(VK_SHIFT) & 0x8000) e.modifiers |= 2;
        if (GetKeyState(VK_MENU) & 0x8000) e.modifiers |= 4;
        break;

    // 窗口大小改变
    case WM_SIZE:
        width_ = static_cast<int>(LOWORD(lParam));
        height_ = static_cast<int>(HIWORD(lParam));

        // 重新创建位图缓冲区
        if (bitmap_) {
            DeleteObject(bitmap_);
            bitmap_ = nullptr;
            bitmapBits_ = nullptr;
        }

        e.type = Event::Type::WindowResize;
        e.width = width_;
        e.height = height_;
        break;

    // 窗口关闭
    case WM_CLOSE: {
        RawEvent raw;
        raw.device = RawEvent::Device::Window;
        raw.action = RawEvent::Action::WindowClose;
        raw.timestamp = GetTickCount64();
        rawCallback_(raw);
    }
        Destroy();
        return 0;

    // 窗口绘制
    case WM_PAINT: ValidateRect(hwnd_, nullptr); return 0;
    // 跨缩放率屏幕拖动：系统通知到达即重算。仅采纳建议位置（保持跟手），
    // 尺寸统一交既有 RefitToNearestMonitor() 重算（含夹紧与装饰补偿，逻辑零改动）
    case WM_DPICHANGED: {
        RECT *suggested = (RECT *)lParam;
        if (suggested)
            SetWindowPos(hwnd_, NULL, suggested->left, suggested->top, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        RefitToNearestMonitor();
        // 本轮系统已介入改窗：同步基线，防 EXITSIZEMOVE 误判为手动缩放
        GetWindowRect(hwnd_, &enterRect_);
        moveTrackMon_ = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        moveCrossed_ = false;
        return 0;
    }
    // 拖动中持续跟踪所在屏：相邻两次位置异屏即标记迁移。
    // 纯观察者：只记两个标量，消息照常交还 DefWindowProc
    case WM_MOVE: {
        HMONITOR m = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        if (moveTrackMon_ && m != moveTrackMon_) moveCrossed_ = true;
        moveTrackMon_ = m;
        return DefWindowProcW(hwnd_, msg, wParam, lParam);
    }
    case WM_ENTERSIZEMOVE:
        GetWindowRect(hwnd_, &enterRect_);    // 记录进入模态循环时的窗口尺寸
        moveTrackMon_ = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        moveCrossed_ = false;
        return DefWindowProcW(hwnd_, msg, wParam, lParam);
    case WM_EXITSIZEMOVE: {
        // 本次循环中尺寸未变 ⇒ 纯位置拖动 → 交由 Refit 跨屏重算占屏；
        // 尺寸被用户改变 ⇒ 手动缩放结果，完全尊重，不回弹
        RECT now{};
        GetWindowRect(hwnd_, &now);
        bool sizeKept = std::abs(int(now.right - now.left) - int(enterRect_.right - enterRect_.left)) <= 1
                        && std::abs(int(now.bottom - now.top) - int(enterRect_.bottom - enterRect_.top)) <= 1;
        bool movedAcross = moveCrossed_;                         // 全程曾跨屏，对循环重启免疫
        if (sizeKept && movedAcross) RefitToNearestMonitor();    // 仅跨屏迁移才重算
        moveCrossed_ = false;
        moveTrackMon_ = nullptr;
        return DefWindowProcW(hwnd_, msg, wParam, lParam);
    }
    default: return DefWindowProcW(hwnd_, msg, wParam, lParam);
    }

    // 构造 RawEvent 并回调:
    RawEvent raw;
    raw.device = RawEvent::Device::Mouse;    // 或 Keyboard/Window
    raw.timestamp = GetTickCount64();

    switch (e.type) {
    case Event::Type::MouseMove:
        raw.action = RawEvent::Action::Move;
        raw.x = static_cast<float>(e.x);
        raw.y = static_cast<float>(e.y);
        raw.pointerId = 0;
        break;
    case Event::Type::MouseDown:
        raw.action = RawEvent::Action::Down;
        raw.x = static_cast<float>(e.x);
        raw.y = static_cast<float>(e.y);
        raw.pointerId = 0;
        break;
    case Event::Type::MouseUp:
        raw.action = RawEvent::Action::Up;
        raw.x = static_cast<float>(e.x);
        raw.y = static_cast<float>(e.y);
        raw.pointerId = 0;
        break;
    case Event::Type::MouseWheel:
        raw.action = RawEvent::Action::Scroll;
        raw.scrollY = GET_WHEEL_DELTA_WPARAM(wParam) / 120.0f;
        raw.x = static_cast<float>(e.x);
        raw.y = static_cast<float>(e.y);
        raw.pointerId = 0;
        break;
    case Event::Type::KeyDown:
        raw.device = RawEvent::Device::Keyboard;
        raw.action = RawEvent::Action::KeyDown;
        raw.keyCode = e.keyCode;
        raw.modifiers = e.modifiers;
        break;
    case Event::Type::KeyUp:
        raw.device = RawEvent::Device::Keyboard;
        raw.action = RawEvent::Action::KeyUp;
        raw.keyCode = e.keyCode;
        raw.modifiers = e.modifiers;
        break;
    case Event::Type::TextInput:
        raw.device = RawEvent::Device::Keyboard;
        raw.action = RawEvent::Action::TextInput;
        raw.charCode = e.charCode;
        break;
    case Event::Type::WindowResize:
        raw.device = RawEvent::Device::Window;
        raw.action = RawEvent::Action::WindowResize;
        raw.width = e.width;
        raw.height = e.height;
        break;
    case Event::Type::WindowClose:
        raw.device = RawEvent::Device::Window;
        raw.action = RawEvent::Action::WindowClose;
        break;
        // TouchBegin/TouchMove/TouchEnd/TouchCancel (Android):
        // raw.device = RawEvent::Device::Touch;
        // raw.pointerId = e.touchId;
        // raw.pressure = e.pressure;
    default: break;
    }

    if (rawCallback_) rawCallback_(raw);

    return 0;
}

float PlatformWindowWin32::GetDpiScale() const {
    if (hwnd_) {
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32) {
            auto pfn = (UINT(WINAPI *)(HWND))GetProcAddress(hUser32, "GetDpiForWindow");
            if (pfn) return pfn(hwnd_) / 96.0f;
        }
        HDC hdc = GetDC(hwnd_);
        float dpi = (float)GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(hwnd_, hdc);
        return dpi / 96.0f;
    }
    return 1.0f;
}

void PlatformWindowWin32::GetDesignSize(int *width, int *height) const {
    // Create() 已保存未经缩放的设计尺寸（designWidth_/designHeight_ 成员）
    if (width) *width = designWidth_;
    if (height) *height = designHeight_;
}

void PlatformWindowWin32::GetScreenWorkArea(int *width, int *height) {
    // 窗口所在屏的工作区（非主屏）：跨屏/最大化时内容系数跟随所在屏
    HMONITOR hMon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    if (hMon && GetMonitorInfoW(hMon, &mi)) {
        if (width) *width = mi.rcWork.right - mi.rcWork.left;
        if (height) *height = mi.rcWork.bottom - mi.rcWork.top;
        return;
    }
    // 兜底：查询失败退回主屏
    RECT wa{};
    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0)) {
        if (width) *width = wa.right - wa.left;
        if (height) *height = wa.bottom - wa.top;
    } else {
        if (width) *width = GetSystemMetrics(SM_CXSCREEN);
        if (height) *height = GetSystemMetrics(SM_CYSCREEN);
    }
}

bool PlatformWindowWin32::GetClipboardText(std::string &out) {
    if (!OpenClipboard(hwnd_)) return false;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (!h) {
        CloseClipboard();
        return false;
    }
    const wchar_t *w = static_cast<const wchar_t *>(GlobalLock(h));
    if (!w) {
        CloseClipboard();
        return false;
    }
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (len > 1) {
        out.resize(len - 1);
        WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), len, nullptr, nullptr);
    }
    GlobalUnlock(h);
    CloseClipboard();
    return true;
}

void PlatformWindowWin32::SetClipboardText(const std::string &text) {
    if (!OpenClipboard(hwnd_)) return;
    EmptyClipboard();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)wlen * sizeof(wchar_t))) {
        wchar_t *w = static_cast<wchar_t *>(GlobalLock(h));
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, w, wlen);
        GlobalUnlock(h);
        SetClipboardData(CF_UNICODETEXT, h);
    }
    CloseClipboard();
}

void PlatformWindowWin32::SetRawEventCallback(PlatformWindow::RawEventCallback callback) {
    rawCallback_ = std::move(callback);
}

static RawEvent toRawEvent(const Event &e, UINT msg, WPARAM wParam, LPARAM) {
    RawEvent r;
    r.timestamp = GetTickCount64();
    switch (e.type) {
    case Event::Type::MouseMove:
        r.device = RawEvent::Device::Mouse;
        r.action = RawEvent::Action::Move;
        r.x = (float)e.x;
        r.y = (float)e.y;
        r.pointerId = 0;
        break;
    case Event::Type::MouseDown:
        r.device = RawEvent::Device::Mouse;
        r.action = RawEvent::Action::Down;
        r.x = (float)e.x;
        r.y = (float)e.y;
        r.pointerId = 0;
        break;
    case Event::Type::MouseUp:
        r.device = RawEvent::Device::Mouse;
        r.action = RawEvent::Action::Up;
        r.x = (float)e.x;
        r.y = (float)e.y;
        r.pointerId = 0;
        break;
    case Event::Type::MouseWheel:
        r.device = RawEvent::Device::Mouse;
        r.action = RawEvent::Action::Scroll;
        r.scrollY = (float)GET_WHEEL_DELTA_WPARAM(wParam) / 120.0f;
        r.x = (float)e.x;
        r.y = (float)e.y;
        r.pointerId = 0;
        break;
    case Event::Type::KeyDown:
        r.device = RawEvent::Device::Keyboard;
        r.action = RawEvent::Action::KeyDown;
        r.keyCode = e.keyCode;
        r.modifiers = e.modifiers;
        break;
    case Event::Type::TextInput:
        r.device = RawEvent::Device::Keyboard;
        r.action = RawEvent::Action::TextInput;
        r.charCode = e.charCode;
        break;
    case Event::Type::WindowResize:
        r.device = RawEvent::Device::Window;
        r.action = RawEvent::Action::WindowResize;
        r.width = e.width;
        r.height = e.height;
        break;
    case Event::Type::WindowClose:
        r.device = RawEvent::Device::Window;
        r.action = RawEvent::Action::WindowClose;
        break;
    default: break;
    }
    return r;
}

void PlatformWindowWin32::RefitToNearestMonitor() {
    HMONITOR hMon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    if (!hMon) return;
    MONITORINFO mi{sizeof(mi)};
    if (!GetMonitorInfoW(hMon, &mi)) return;

    // 目标屏固有系数：S0 = min(工作区宽÷1920, 工作区高÷1080)
    float sw = float(mi.rcWork.right - mi.rcWork.left) / kBaselineScreenW;
    float sh = float(mi.rcWork.bottom - mi.rcWork.top) / kBaselineScreenH;
    float s0 = sw < sh ? sw : sh;

    // 客户区目标 = 设计稿 × S0（与 Create 初值公式同源）
    int tgtW = int(designWidth_ * s0);
    int tgtH = int(designHeight_ * s0);
    RECT rc{0, 0, tgtW, tgtH};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    // 锚定松手处，夹进目标屏工作区
    RECT now{};
    GetWindowRect(hwnd_, &now);
    const int waL = int(mi.rcWork.left), waR = int(mi.rcWork.right);
    const int waT = int(mi.rcWork.top), waB = int(mi.rcWork.bottom);
    const int winW = rc.right - rc.left, winH = rc.bottom - rc.top;

    int x = std::max(waL, std::min(int(now.left), waR - winW));
    int y = std::max(waT, std::min(int(now.top), waB - winH));
    SetWindowPos(hwnd_, NULL, x, y, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER | SWP_NOACTIVATE);

    // 二次校正：PMv2 下非客户区按目标屏实际缩放，实测客户区增量补偿装饰误差
    for (int i = 0; i < 2; ++i) {
        RECT cli{};
        GetClientRect(hwnd_, &cli);
        int dW = tgtW - (cli.right - cli.left);
        int dH = tgtH - (cli.bottom - cli.top);
        if (std::abs(dW) <= 1 && std::abs(dH) <= 1) break;
        RECT cur{};
        GetWindowRect(hwnd_, &cur);
        SetWindowPos(hwnd_, NULL, 0, 0, cur.right - cur.left + dW, cur.bottom - cur.top + dH,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}