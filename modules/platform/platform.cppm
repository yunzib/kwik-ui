export module kwik.platform;
export import kwik.platform.window;

// 按 CMake Platform.cmake 选定的后端重导出实现模块（KWIK_PLATFORM_<backend>
// 编译定义是唯一事实源）。调用方不经过工厂——直接构造具体类型：
//   auto win = std::make_unique<PlatformWindowWin32>();   // Windows
// 平台选择由构建系统在库粒度完成（每平台只编一个后端翻译单元），
// 调用点（main/example）本就是平台特定入口，直接类型比 #ifdef 工厂诚实。
// 注意：window_factory 已删除——其函数体内 import 为非法 C++，且全仓
// 调用点均已是直接构造（零调用死代码）。
#if defined(KWIK_PLATFORM_win32)
export import kwik.platform.win32_window;
#elif defined(KWIK_PLATFORM_wayland)
export import kwik.platform.wayland_window;
#elif defined(KWIK_PLATFORM_drm)
export import kwik.platform.drm_window;
#elif defined(KWIK_PLATFORM_fbdev)
export import kwik.platform.fbdev_window;
#elif defined(KWIK_PLATFORM_android)
export import kwik.platform.android_window;
#elif defined(KWIK_PLATFORM_harmony)
export import kwik.platform.harmony_window;
#elif defined(KWIK_PLATFORM_cocoa)
export import kwik.platform.cocoa_window;
#elif defined(KWIK_PLATFORM_ios)
export import kwik.platform.ios_window;
#endif
