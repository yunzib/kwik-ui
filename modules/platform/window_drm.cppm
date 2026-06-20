module;
export module kwik.platform.drm_window;
#if defined(__linux__) && defined(USE_DRM)
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <linux/input.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <mutex>
#include <map>

import kwik.platform.platform_window;
import kwik.core.types;
import std;
export namespace kwik::platform {
    /**
    * @brief Linux DRM/KMS平台窗口实现
    * 
    * 使用Direct Rendering Manager和Kernel Mode Setting进行直接硬件渲染
    * 适用于嵌入式系统和无窗口管理器的Linux环境
    */
    class PlatformWindowDRM : public PlatformWindow {
    public:
        PlatformWindowDRM();
        ~PlatformWindowDRM() override;
        
        // 窗口生命周期
        bool Create(const std::string& title, int width, int height) override;
        void Destroy() override;
        void Show() override;
        void Hide() override;
        void GetSize(int* width, int* height) const override;
        
        // 软件渲染
        bool LockBackBuffer(void** pixels, int* stride) override;
        void UnlockBackBuffer() override;
        void Present() override;
        
        // GPU渲染
        void* GetNativeHandle() const override;
        
        // 事件处理
        void SetEventCallback(EventCallback callback) override;
        void PollEvents() override;
        void WaitEvents() override;
        
        // 窗口定制
        void SetDecoration(WindowDecoration decoration) override;
        void SetShape(const std::vector<std::pair<int, int>>& polygon) override;
        void SetShapeMask(const uint8_t* maskData, int width, int height) override;
        void SetResizable(bool resizable) override;
        
    private:
        // DRM/KMS相关
        int drmFd_ = -1;
        drmModeModeInfo mode_;
        uint32_t connectorId_ = 0;
        uint32_t encoderId_ = 0;
        uint32_t crtcId_ = 0;
        drmModeCrtcPtr savedCrtc_ = nullptr;
        
        // GBM/EGL相关
        gbm_device* gbm_ = nullptr;
        gbm_surface* gbmSurface_ = nullptr;
        EGLDisplay eglDisplay_ = EGL_NO_DISPLAY;
        EGLContext eglContext_ = EGL_NO_CONTEXT;
        EGLSurface eglSurface_ = EGL_NO_SURFACE;
        
        // 软件渲染缓冲区
        struct DumbBuffer {
            uint32_t handle = 0;
            uint32_t pitch = 0;
            uint64_t size = 0;
            void* map = nullptr;
            uint32_t fbId = 0;
        } dumb_;
        
        // 输入处理
        int evdevFd_ = -1;
        std::thread inputThread_;
        std::mutex inputMutex_;
        bool running_ = false;
        void InputThreadFunc();
        
        // 窗口状态
        int width_ = 0;
        int height_ = 0;
        EventCallback callback_;
        WindowDecoration decoration_ = WindowDecoration::Normal;
        bool resizable_ = false;
        
        // 辅助方法
        bool FindDisplayResources();
        bool SetupEGL();
        bool SetupDumbBuffer();
        void ProcessEvdevEvent(const struct input_event& ev);
        static Event::MouseButton MapEvdevButton(uint32_t code);
        static uint32_t MapEvdevKey(uint32_t code);
    };
}
#endif // __linux__ && USE_DRM