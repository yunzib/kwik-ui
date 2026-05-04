export module kwik.platform;
export import kwik.platform.window;
export import kwik.platform.window_factory;
// 根据平台导出相应的实现模块
#if defined(_WIN32)
    export import kwik.platform.win32_window;
#elif defined(__linux__)
    #if defined(USE_DRM)
        export import kwik.platform.drm_window;
    #elif defined(USE_FBDEV)
        export import kwik.platform.fbdev_window;
    #else
        export import kwik.platform.wayland_window;
    #endif
#elif defined(__ANDROID__)
    export import kwik.platform.android_window;
#endif