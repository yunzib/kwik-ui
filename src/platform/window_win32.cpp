module; // 全局模块片段开始

#include <windows.h>
#include <map>

module kwik.platform.win32_window;

import kwik.utils.string_utils;

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
    // 计算窗口大小（包含边框）
    RECT rect = {0, 0, width, height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    int windowWidth = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;

    // 居中显示
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenWidth - windowWidth) / 2;
    int posY = (screenHeight - windowHeight) / 2;
    // 创建窗口
    // std::wstring wtitle(title.begin(), title.end());
    std::wstring wtitle = Utf8ToWide(title);

    hwnd_ = CreateWindowExW(0, L"KwikUIWindowClass", wtitle.c_str(), WS_OVERLAPPEDWINDOW, posX, posY, windowWidth,
                            windowHeight, nullptr, nullptr, GetModuleHandle(nullptr), this);

    if (!hwnd_) { return false; }
    // 注册到全局映射
    g_windowMap[hwnd_] = this;
    hdc_ = GetDC(hwnd_);
    width_ = width;
    height_ = height;

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
        bmi.bmiHeader.biHeight = -height_; // 从上到下
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HDC screenDC = GetDC(nullptr);
        bitmap_ = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bitmapBits_, nullptr, 0);
        ReleaseDC(nullptr, screenDC);
    }

    if (pixels) *pixels = bitmapBits_;
    if (stride) *stride = width_ * 4; // 32bpp

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
    SetWindowRgn(hwnd_, rgn, TRUE); // 窗口获得区域所有权
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
    if (!callback_) { return DefWindowProcW(hwnd_, msg, wParam, lParam); }
    Event e;

    switch (msg) {
    // 鼠标移动
    case WM_MOUSEMOVE:
        e.type = Event::Type::MouseMove;
        e.x = static_cast<int>(LOWORD(lParam));
        e.y = static_cast<int>(HIWORD(lParam));
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
        POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
        ScreenToClient(hwnd_, &pt);
        e.x = pt.x;
        e.y = pt.y;
        break;
    }

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
    case WM_CLOSE:
        e.type = Event::Type::WindowClose;
        callback_(e);
        Destroy();
        return 0;

    // 窗口绘制
    case WM_PAINT: ValidateRect(hwnd_, nullptr); return 0;

    default: return DefWindowProcW(hwnd_, msg, wParam, lParam);
    }

    callback_(e);
    return 0;
}
