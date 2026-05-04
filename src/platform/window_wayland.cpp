module kwik.platform.wayland_window;

export namespace kwik::platform {
    // ============================================================================
    // Wayland监听器回调实现
    // ============================================================================
    void PlatformWindowWayland::RegistryHandleGlobal(void* data, wl_registry* registry,
                                                    uint32_t name, const char* interface, 
                                                    uint32_t version) {
        auto* win = static_cast<PlatformWindowWayland*>(data);
        
        if (strcmp(interface, wl_compositor_interface.name) == 0) {
            win->compositor_ = static_cast<wl_compositor*>(
                wl_registry_bind(registry, name, &wl_compositor_interface, 1));
        } else if (strcmp(interface, wl_shm_interface.name) == 0) {
            win->shm_ = static_cast<wl_shm*>(
                wl_registry_bind(registry, name, &wl_shm_interface, 1));
        } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
            win->xdg_wm_base_ = static_cast<xdg_wm_base*>(
                wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
            xdg_wm_base_add_listener(win->xdg_wm_base_, &xdg_wm_base_listener, win);
        } else if (strcmp(interface, wl_seat_interface.name) == 0) {
            win->seat_ = static_cast<wl_seat*>(
                wl_registry_bind(registry, name, &wl_seat_interface, 1));
        }
    }

    void PlatformWindowWayland::RegistryHandleGlobalRemove(void* data, 
                                                        wl_registry* registry, 
                                                        uint32_t name) {
        // 处理全局对象移除
    }

    void PlatformWindowWayland::XdgWmBaseHandlePing(void* data, xdg_wm_base* xdg_wm_base, 
                                                    uint32_t serial) {
        xdg_wm_base_pong(xdg_wm_base, serial);
    }

    void PlatformWindowWayland::XdgSurfaceHandleConfigure(void* data, xdg_surface* xdg_surface, 
                                                        uint32_t serial) {
        xdg_surface_ack_configure(xdg_surface, serial);
    }

    void PlatformWindowWayland::XdgToplevelHandleConfigure(void* data, xdg_toplevel* toplevel,
                                                        int32_t width, int32_t height, 
                                                        wl_array* states) {
        auto* win = static_cast<PlatformWindowWayland*>(data);
        
        if (width > 0 && height > 0) {
            win->width_ = width;
            win->height_ = height;
            
            // 通知窗口大小改变
            if (win->callback_) {
                Event e;
                e.type = Event::Type::WindowResize;
                e.width = width;
                e.height = height;
                win->callback_(e);
            }
        }
    }

    void PlatformWindowWayland::XdgToplevelHandleClose(void* data, xdg_toplevel* toplevel) {
        auto* win = static_cast<PlatformWindowWayland*>(data);
        
        if (win->callback_) {
            Event e;
            e.type = Event::Type::WindowClose;
            win->callback_(e);
        }
        win->Destroy();
    }

    void PlatformWindowWayland::PointerHandleEnter(void* data, wl_pointer* pointer, 
                                                    uint32_t serial, wl_surface* surface,
                                                    wl_fixed_t sx, wl_fixed_t sy) {
        // 鼠标进入窗口
    }

    void PlatformWindowWayland::PointerHandleLeave(void* data, wl_pointer* pointer,
                                                    uint32_t serial, wl_surface* surface) {
        // 鼠标离开窗口
    }

    void PlatformWindowWayland::PointerHandleMotion(void* data, wl_pointer* pointer,
                                                    uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
        auto* win = static_cast<PlatformWindowWayland*>(data);
        
        if (win->callback_) {
            Event e;
            e.type = Event::Type::MouseMove;
            e.x = wl_fixed_to_int(sx);
            e.y = wl_fixed_to_int(sy);
            win->callback_(e);
        }
    }

    void PlatformWindowWayland::PointerHandleButton(void* data, wl_pointer* pointer,
                                                    uint32_t serial, uint32_t time,
                                                    uint32_t button, uint32_t state) {
        auto* win = static_cast<PlatformWindowWayland*>(data);
        
        if (win->callback_) {
            Event e;
            e.type = (state == WL_POINTER_BUTTON_STATE_PRESSED) 
                    ? Event::Type::MouseDown : Event::Type::MouseUp;
            
            // Wayland按钮映射
            switch (button) {
                case BTN_LEFT:   e.button = Event::MouseButton::Left; break;
                case BTN_RIGHT:  e.button = Event::MouseButton::Right; break;
                case BTN_MIDDLE: e.button = Event::MouseButton::Middle; break;
                default: break;
            }
            
            win->callback_(e);
        }
    }

    // ============================================================================
    // 构造与析构
    // ============================================================================
    PlatformWindowWayland::PlatformWindowWayland() = default;
    PlatformWindowWayland::~PlatformWindowWayland() {
        Destroy();
    }

    // ============================================================================
    // 窗口生命周期
    // ============================================================================
    bool PlatformWindowWayland::Create(const std::string& title, int width, int height) {
        // 连接到Wayland显示服务器
        display_ = wl_display_connect(nullptr);
        if (!display_) {
            return false;
        }
        // 获取注册表并绑定全局对象
        registry_ = wl_display_get_registry(display_);
        wl_registry_add_listener(registry_, &registry_listener, this);
        wl_display_roundtrip(display_);
        // 检查必需的全局对象
        if (!compositor_ || !shm_ || !xdg_wm_base_) {
            Destroy();
            return false;
        }
        // 创建Wayland表面
        surface_ = wl_compositor_create_surface(compositor_);
        
        // 创建XDG表面
        xdg_surface_ = xdg_wm_base_get_xdg_surface(xdg_wm_base_, surface_);
        xdg_surface_add_listener(xdg_surface_, &xdg_surface_listener, this);
        
        // 创建XDG顶层
        xdg_toplevel_ = xdg_surface_get_toplevel(xdg_surface_);
        xdg_toplevel_add_listener(xdg_toplevel_, &xdg_toplevel_listener, this);
        xdg_toplevel_set_title(xdg_toplevel_, title.c_str());
        
        // 提交表面配置
        wl_surface_commit(surface_);
        wl_display_roundtrip(display_);
        width_ = width;
        height_ = height;
        
        return true;
    }

    void PlatformWindowWayland::Destroy() {
        // 释放缓冲区
        if (buffer_) {
            wl_buffer_destroy(buffer_);
            buffer_ = nullptr;
        }
        if (pool_) {
            wl_shm_pool_destroy(pool_);
            pool_ = nullptr;
        }
        if (shm_data_) {
            munmap(shm_data_, shm_size_);
            shm_data_ = nullptr;
        }
        
        // 释放Wayland对象
        if (xdg_toplevel_) {
            xdg_toplevel_destroy(xdg_toplevel_);
            xdg_toplevel_ = nullptr;
        }
        if (xdg_surface_) {
            xdg_surface_destroy(xdg_surface_);
            xdg_surface_ = nullptr;
        }
        if (surface_) {
            wl_surface_destroy(surface_);
            surface_ = nullptr;
        }
        if (display_) {
            wl_display_disconnect(display_);
            display_ = nullptr;
        }
    }

    void PlatformWindowWayland::Show() {
        // Wayland窗口创建后自动显示
    }

    void PlatformWindowWayland::Hide() {
        // Wayland没有明确的隐藏功能
    }

    void PlatformWindowWayland::GetSize(int* width, int* height) const {
        if (width) *width = width_;
        if (height) *height = height_;
    }

    // ============================================================================
    // 软件渲染接口
    // ============================================================================
    /**
    * @brief 创建共享内存文件
    */
    static int CreateShmFile(size_t size) {
        char name[] = "/kwik_shm-XXXXXX";
        int fd = mkstemp(name);
        if (fd < 0) return -1;
        
        if (ftruncate(fd, static_cast<off_t>(size)) < 0) {
            close(fd);
            return -1;
        }
        
        unlink(name);
        return fd;
    }

    bool PlatformWindowWayland::LockBackBuffer(void** pixels, int* stride) {
        int stride_val = width_ * 4;
        size_t size = static_cast<size_t>(stride_val * height_);
        
        // 创建缓冲区（如果尚未创建）
        if (!buffer_) {
            int fd = CreateShmFile(size);
            if (fd < 0) return false;
            
            shm_data_ = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (shm_data_ == MAP_FAILED) {
                close(fd);
                return false;
            }
            
            pool_ = wl_shm_create_pool(shm_, fd, static_cast<int32_t>(size));
            buffer_ = wl_shm_pool_create_buffer(pool_, 0, width_, height_, 
                                                stride_val, WL_SHM_FORMAT_ARGB8888);
            
            // 池子用完即销毁，缓冲区仍有效
            wl_shm_pool_destroy(pool_);
            pool_ = nullptr;
            close(fd);
            
            shm_size_ = size;
        }
        
        if (pixels) *pixels = shm_data_;
        if (stride) *stride = stride_val;
        
        return true;
    }

    void PlatformWindowWayland::UnlockBackBuffer() {
        // 不需要解锁操作
    }

    void PlatformWindowWayland::Present() {
        if (!buffer_) return;
        
        // 附加缓冲区并提交
        wl_surface_attach(surface_, buffer_, 0, 0);
        wl_surface_damage(surface_, 0, 0, width_, height_);
        wl_surface_commit(surface_);
        wl_display_flush(display_);
    }

    // ============================================================================
    // 事件处理
    // ============================================================================
    void PlatformWindowWayland::PollEvents() {
        wl_display_dispatch_pending(display_);
        wl_display_flush(display_);
    }

    void PlatformWindowWayland::WaitEvents() {
        wl_display_dispatch(display_);
    }

    // ============================================================================
    // 工厂函数
    // ============================================================================
    std::unique_ptr<PlatformWindow> CreatePlatformWindow() {
        return std::make_unique<PlatformWindowWayland>();
    }

}

