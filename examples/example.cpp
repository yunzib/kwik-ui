
#if defined(_WIN32)
#include <Windows.h>
#endif
#include <iostream>
#include <string>

import kwik.platform.window;
import kwik.platform.win32_window;
import kwik.app;

static std::string resolveDemo(int argc, char *argv[]) {
    if (argc >= 2) {
        std::string arg = argv[1];
        if (arg == "view") return "../../examples/view.js";
        if (arg == "text") return "../../examples/text.js";
        if (arg == "event") return "../../examples/event.js";
        if (arg == "flex") return "../../examples/flex.js";
        if (arg == "list") return "../../examples/list.js";
        if (arg == "grid") return "../../examples/grid.js";
        if (arg == "stack") return "../../examples/stack.js";
        return arg;
    }
    return "../../examples/view.js";
}

int main(int argc, char *argv[]) {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    auto window = std::make_unique<PlatformWindowWin32>();
    if (!window || !window->Create("KwiK UI Demo", 800, 600)) return -1;
    window->Show();
    Application app(*window, {
                                 .jsPath = resolveDemo(argc, argv),
                                 .fontDirs = {"../../resources/fonts"},
                                 .width = 800,
                                 .height = 600,
                             });
    return app.run();
}

// 如果需和 JS 交互：
// Application app(*window, { .jsPath = "app.js", ... });
// app.jsContext().evalScript("globalConfig = {...}");  // run() 之前注入
// return app.run();
// 或自定义事件：
// Application app(*window, {
//     ...
//     .onEvent = [](const Event& e) -> bool {
//         if (e.type == Event::Type::KeyDown && e.key == VK_ESCAPE)
//             return true;  // 拦截 Esc, 不交给默认管线
//         return false;
//     }
// });