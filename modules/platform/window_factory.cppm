export module kwik.platform.window_factory;


import std;
import kwik.platform.window;
import kwik.platform.win32_window;

export namespace kwik::platform {
    std::unique_ptr<PlatformWindow> CreatePlatformWindow() {
        #if defined(_WIN32)
            return std::make_unique<PlatformWindowWin32>();
            
        #elif defined(__linux__)
            #if defined(USE_DRM)
                import kwik.platform.drm_window;
                return std::make_unique<PlatformWindowDRM>();
                
            #elif defined(USE_FBDEV)
                import kwik.platform.fbdev_window;
                return std::make_unique<PlatformWindowFBDev>();
                
            #else
                import kwik.platform.wayland_window;
                return std::make_unique<PlatformWindowWayland>();
                
            #endif
            
        #elif defined(__ANDROID__)
            import kwik.platform.android_window;
            return std::make_unique<PlatformWindowAndroid>();
            
        #elif defined(__APPLE__)
            // macOS/iOS平台（需要额外实现）
            #if TARGET_OS_IOS
                // std::unique_ptr<PlatformWindow> CreatePlatformWindow() {
                //     return std::make_unique<PlatformWindowIOS>();
                // }
            #else
                // std::unique_ptr<PlatformWindow> CreatePlatformWindow() {
                //     return std::make_unique<PlatformWindowCocoa>();
                // }
            #endif
            
        #else
            #error "不支持的平台"
        #endif
    }
}