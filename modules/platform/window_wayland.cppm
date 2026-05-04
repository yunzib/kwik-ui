module;

#if defined(__linux__) && !defined(USE_DRM) && !defined(USE_FBDEV)
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xdg-shell.h>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <mutex>

export module kwik.platform.wayland_window;

import kwik.platform.platform_window;
import kwik.platform.platform_window;
import kwik.core.types;
import std;

export namespace kwik::platform {
    /**
    * @brief Linux Wayland平台窗口实现
    */
    class PlatformWindowWayland : public PlatformWindow {
    public:
        PlatformWindowWayland();
        ~PlatformWindowWayland() override;
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
        void* GetNativeHandle() const override { return static_cast<void*>(surface_); }
        // 事件处理
        void SetEventCallback(EventCallback callback) override { callback_ = std::move(callback); }
        void PollEvents() override;
        void WaitEvents() override;
        // 窗口定制（Wayland限制较多）
        void SetDecoration(WindowDecoration decoration) override {}
        void SetShape(const std::vector<std::pair<int, int>>& polygon) override {}
        void SetShapeMask(const uint8_t* maskData, int width, int height) override {}
        void SetResizable(bool resizable) override {}
    private:
        // Wayland核心对象
        wl_display* display_ = nullptr;
        wl_registry* registry_ = nullptr;
        wl_compositor* compositor_ = nullptr;
        wl_shm* shm_ = nullptr;
        xdg_wm_base* xdg_wm_base_ = nullptr;
        wl_surface* surface_ = nullptr;
        xdg_surface* xdg_surface_ = nullptr;
        xdg_toplevel* xdg_toplevel_ = nullptr;
        
        // 输入设备
        wl_seat* seat_ = nullptr;
        wl_pointer* pointer_ = nullptr;
        wl_keyboard* keyboard_ = nullptr;
        wl_touch* touch_ = nullptr;
        // 软件渲染缓冲区
        wl_shm_pool* pool_ = nullptr;
        wl_buffer* buffer_ = nullptr;
        void* shm_data_ = nullptr;
        size_t shm_size_ = 0;
        int width_ = 0;
        int height_ = 0;
        EventCallback callback_;
        
        // 事件处理回调
        static void RegistryHandleGlobal(void* data, wl_registry* registry, 
                                        uint32_t name, const char* interface, uint32_t version);
        static void RegistryHandleGlobalRemove(void* data, wl_registry* registry, uint32_t name);
        static void XdgWmBaseHandlePing(void* data, xdg_wm_base* xdg_wm_base, uint32_t serial);
        static void XdgSurfaceHandleConfigure(void* data, xdg_surface* xdg_surface, uint32_t serial);
        static void XdgToplevelHandleConfigure(void* data, xdg_toplevel* toplevel,
                                            int32_t width, int32_t height, wl_array* states);
        static void XdgToplevelHandleClose(void* data, xdg_toplevel* toplevel);
        static void PointerHandleEnter(void* data, wl_pointer* pointer, uint32_t serial,
                                    wl_surface* surface, wl_fixed_t sx, wl_fixed_t sy);
        static void PointerHandleLeave(void* data, wl_pointer* pointer, uint32_t serial,
                                    wl_surface* surface);
        static void PointerHandleMotion(void* data, wl_pointer* pointer, uint32_t time,
                                        wl_fixed_t sx, wl_fixed_t sy);
        static void PointerHandleButton(void* data, wl_pointer* pointer, uint32_t serial,
                                        uint32_t time, uint32_t button, uint32_t state);
    };

    // ============================================================================
    // Wayland监听器定义
    // ============================================================================
    const wl_registry_listener registry_listener = {
        PlatformWindowWayland::RegistryHandleGlobal,
        PlatformWindowWayland::RegistryHandleGlobalRemove
    };
    const xdg_wm_base_listener xdg_wm_base_listener = {
        PlatformWindowWayland::XdgWmBaseHandlePing
    };
    const xdg_surface_listener xdg_surface_listener = {
        PlatformWindowWayland::XdgSurfaceHandleConfigure
    };
    const xdg_toplevel_listener xdg_toplevel_listener = {
        PlatformWindowWayland::XdgToplevelHandleConfigure,
        PlatformWindowWayland::XdgToplevelHandleClose
    };
    const wl_pointer_listener pointer_listener = {
        .enter = PlatformWindowWayland::PointerHandleEnter,
        .leave = PlatformWindowWayland::PointerHandleLeave,
        .motion = PlatformWindowWayland::PointerHandleMotion,
        .button = PlatformWindowWayland::PointerHandleButton,
        .axis = nullptr,
        .frame = nullptr,
        .axis_source = nullptr,
        .axis_stop = nullptr,
        .axis_discrete = nullptr
    };
}

#endif // __linux__ && !USE_DRM && !USE_FBDEV