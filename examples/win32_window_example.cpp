#if defined(_WIN32)
#include <Windows.h>
#endif
#include <cstdio>

import kwik.platform.window;
import kwik.platform.win32_window;
import std;

int main() {
    auto window = std::make_unique<PlatformWindowWin32>();
    if (!window || !window->Create("Window GDI Test", 800, 600)) return -1;
    window->Show();

    bool running = true;
    int maxCycle = 0;

    window->SetEventCallback([&](const Event& e) {
        if (e.type == Event::Type::WindowClose) running = false;
        if (e.type == Event::Type::WindowResize) {
            HWND hwnd = (HWND)window->GetNativeHandle();
            bool isMax = IsZoomed(hwnd);
            if (isMax) {
                maxCycle++;
                printf("[Resize] %dx%d MAXIMIZED (#%d)\n", e.width, e.height, maxCycle);
            } else {
                printf("[Resize] %dx%d RESTORED (#%d)\n", e.width, e.height, maxCycle);
            }
        }
    });

    while (running) {
        window->PollEvents();
        HWND hwnd = (HWND)window->GetNativeHandle();
        HDC hdc = GetDC(hwnd);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH brush = CreateSolidBrush(RGB(0xF5, 0xF5, 0xF5));
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);
        ReleaseDC(hwnd, hdc);
    }
    return 0;
}