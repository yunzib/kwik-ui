module kwik.platform.drm_window;
#include <iostream>
#include <dirent.h>
#include <poll.h>
namespace kwik::platform {
PlatformWindowDRM::PlatformWindowDRM() = default;
PlatformWindowDRM::~PlatformWindowDRM() {
    Destroy();
}
bool PlatformWindowDRM::Create(const std::string& title, int width, int height) {
    // 1. 打开DRM设备
    const char* drmDevices[] = {"/dev/dri/card0", "/dev/dri/card1", "/dev/dri/card2"};
    for (const char* device : drmDevices) {
        drmFd_ = open(device, O_RDWR | O_CLOEXEC);
        if (drmFd_ >= 0) {
            uint64_t hasDumb;
            if (drmGetCap(drmFd_, DRM_CAP_DUMB_BUFFER, &hasDumb) == 0 && hasDumb) {
                break;
            }
            close(drmFd_);
            drmFd_ = -1;
        }
    }
    
    if (drmFd_ < 0) {
        std::cerr << "Failed to open DRM device" << std::endl;
        return false;
    }
    
    // 2. 查找显示资源
    if (!FindDisplayResources()) {
        Destroy();
        return false;
    }
    
    // 3. 设置窗口尺寸
    if (width > 0 && height > 0) {
        width_ = width;
        height_ = height;
    } else {
        width_ = mode_.hdisplay;
        height_ = mode_.vdisplay;
    }
    
    // 4. 保存当前CRTC状态
    savedCrtc_ = drmModeGetCrtc(drmFd_, crtcId_);
    
    // 5. 初始化GBM
    gbm_ = gbm_create_device(drmFd_);
    if (!gbm_) {
        std::cerr << "Failed to create GBM device" << std::endl;
        Destroy();
        return false;
    }
    
    // 6. 设置EGL（用于GPU渲染）
    if (!SetupEGL()) {
        Destroy();
        return false;
    }
    
    // 7. 设置dumb buffer（用于软件渲染）
    if (!SetupDumbBuffer()) {
        Destroy();
        return false;
    }
    
    // 8. 打开输入设备
    DIR* dir = opendir("/dev/input");
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (strncmp(ent->d_name, "event", 5) == 0) {
                std::string path = std::string("/dev/input/") + ent->d_name;
                int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
                if (fd >= 0) {
                    char name[256] = "Unknown";
                    ioctl(fd, EVIOCGNAME(sizeof(name)), name);
                    // 优先选择鼠标和键盘设备
                    if (strstr(name, "Mouse") || strstr(name, "mouse") || 
                        strstr(name, "Keyboard") || strstr(name, "keyboard")) {
                        evdevFd_ = fd;
                        break;
                    }
                    close(fd);
                }
            }
        }
        closedir(dir);
    }
    
    // 9. 启动输入线程
    if (evdevFd_ >= 0) {
        running_ = true;
        inputThread_ = std::thread(&PlatformWindowDRM::InputThreadFunc, this);
    }
    
    return true;
}
void PlatformWindowDRM::Destroy() {
    running_ = false;
    if (inputThread_.joinable()) {
        inputThread_.join();
    }
    
    // 恢复原始CRTC
    if (savedCrtc_) {
        drmModeSetCrtc(drmFd_, savedCrtc_->crtc_id, savedCrtc_->buffer_id,
                       savedCrtc_->x, savedCrtc_->y, &connectorId_, 1, &savedCrtc_->mode);
        drmModeFreeCrtc(savedCrtc_);
        savedCrtc_ = nullptr;
    }
    
    // 清理dumb buffer
    if (dumb_.fbId) {
        drmModeRmFB(drmFd_, dumb_.fbId);
    }
    if (dumb_.map) {
        munmap(dumb_.map, dumb_.size);
    }
    if (dumb_.handle) {
        struct drm_mode_destroy_dumb destroy = {.handle = dumb_.handle};
        drmIoctl(drmFd_, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    }
    
    // 清理EGL
    if (eglDisplay_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(eglDisplay_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (eglContext_ != EGL_NO_CONTEXT) eglDestroyContext(eglDisplay_, eglContext_);
        if (eglSurface_ != EGL_NO_SURFACE) eglDestroySurface(eglDisplay_, eglSurface_);
        eglTerminate(eglDisplay_);
    }
    
    // 清理GBM
    if (gbmSurface_) gbm_surface_destroy(gbmSurface_);
    if (gbm_) gbm_device_destroy(gbm_);
    
    // 关闭文件描述符
    if (evdevFd_ >= 0) close(evdevFd_);
    if (drmFd_ >= 0) close(drmFd_);
    
    width_ = height_ = 0;
    callback_ = nullptr;
}
void PlatformWindowDRM::Show() {
    // DRM总是全屏显示，无需额外操作
}
void PlatformWindowDRM::Hide() {
    // DRM无法隐藏，可以恢复原始CRTC
    if (savedCrtc_) {
        drmModeSetCrtc(drmFd_, savedCrtc_->crtc_id, savedCrtc_->buffer_id,
                       savedCrtc_->x, savedCrtc_->y, &connectorId_, 1, &savedCrtc_->mode);
    }
}
void PlatformWindowDRM::GetSize(int* width, int* height) const {
    if (width) *width = width_;
    if (height) *height = height_;
}
bool PlatformWindowDRM::LockBackBuffer(void** pixels, int* stride) {
    if (!dumb_.map) {
        if (!SetupDumbBuffer()) {
            return false;
        }
    }
    
    if (pixels) *pixels = dumb_.map;
    if (stride) *stride = dumb_.pitch;
    return true;
}
void PlatformWindowDRM::UnlockBackBuffer() {
    // DRM dumb buffer在Present时提交，这里无需操作
}
void PlatformWindowDRM::Present() {
    if (!dumb_.fbId) {
        // 创建framebuffer
        uint32_t handles[4] = {dumb_.handle};
        uint32_t pitches[4] = {dumb_.pitch};
        uint32_t offsets[4] = {0};
        
        if (drmModeAddFB2(drmFd_, width_, height_, DRM_FORMAT_ARGB8888,
                          handles, pitches, offsets, &dumb_.fbId, 0) != 0) {
            std::cerr << "Failed to add framebuffer" << std::endl;
            return;
        }
    }
    
    // 页面翻转
    if (drmModePageFlip(drmFd_, crtcId_, dumb_.fbId, DRM_MODE_PAGE_FLIP_EVENT, nullptr) != 0) {
        // 回退到SetCrtc
        drmModeSetCrtc(drmFd_, crtcId_, dumb_.fbId, 0, 0, &connectorId_, 1, &mode_);
    }
}
void* PlatformWindowDRM::GetNativeHandle() const {
    return static_cast<void*>(eglSurface_);
}
void PlatformWindowDRM::SetEventCallback(EventCallback callback) {
    callback_ = std::move(callback);
}
void PlatformWindowDRM::PollEvents() {
    // 输入事件在独立线程中处理，这里无需操作
}
void PlatformWindowDRM::WaitEvents() {
    // 等待垂直同步
    drmEventContext evctx = {};
    evctx.version = DRM_EVENT_CONTEXT_VERSION;
    evctx.page_flip_handler = nullptr;
    
    struct pollfd fds[1];
    fds[0].fd = drmFd_;
    fds[0].events = POLLIN;
    
    poll(fds, 1, -1);
    drmHandleEvent(drmFd_, &evctx);
}
void PlatformWindowDRM::SetDecoration(WindowDecoration decoration) {
    decoration_ = decoration;
    // DRM不支持窗口装饰，但可以设置全屏/无边框
}
void PlatformWindowDRM::SetShape(const std::vector<std::pair<int, int>>& polygon) {
    // DRM不支持异形窗口
}
void PlatformWindowDRM::SetShapeMask(const uint8_t* maskData, int width, int height) {
    // DRM不支持异形窗口
}
void PlatformWindowDRM::SetResizable(bool resizable) {
    resizable_ = resizable;
    // DRM不支持调整窗口大小
}
// ==================== 私有方法实现 ====================
bool PlatformWindowDRM::FindDisplayResources() {
    drmModeRes* resources = drmModeGetResources(drmFd_);
    if (!resources) {
        std::cerr << "Failed to get DRM resources" << std::endl;
        return false;
    }
    
    // 查找第一个连接的显示器
    for (int i = 0; i < resources->count_connectors; ++i) {
        drmModeConnector* connector = drmModeGetConnector(drmFd_, resources->connectors[i]);
        if (connector && connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
            connectorId_ = connector->connector_id;
            mode_ = connector->modes[0]; // 使用第一个可用模式
            
            // 查找编码器
            if (connector->encoder_id) {
                drmModeEncoder* encoder = drmModeGetEncoder(drmFd_, connector->encoder_id);
                if (encoder) {
                    crtcId_ = encoder->crtc_id;
                    encoderId_ = encoder->encoder_id;
                    drmModeFreeEncoder(encoder);
                }
            }
            
            drmModeFreeConnector(connector);
            drmModeFreeResources(resources);
            return crtcId_ != 0;
        }
        if (connector) drmModeFreeConnector(connector);
    }
    
    drmModeFreeResources(resources);
    return false;
}
bool PlatformWindowDRM::SetupEGL() {
    eglDisplay_ = eglGetDisplay((EGLNativeDisplayType)gbm_);
    if (eglDisplay_ == EGL_NO_DISPLAY) {
        std::cerr << "Failed to get EGL display" << std::endl;
        return false;
    }
    
    EGLint major, minor;
    if (!eglInitialize(eglDisplay_, &major, &minor)) {
        std::cerr << "Failed to initialize EGL" << std::endl;
        return false;
    }
    
    // 选择配置
    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    
    EGLConfig config;
    EGLint numConfigs;
    if (!eglChooseConfig(eglDisplay_, configAttribs, &config, 1, &numConfigs)) {
        std::cerr << "Failed to choose EGL config" << std::endl;
        return false;
    }
    
    // 创建表面
    gbmSurface_ = gbm_surface_create(gbm_, width_, height_, 
                                     GBM_FORMAT_ARGB8888, 
                                     GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!gbmSurface_) {
        std::cerr << "Failed to create GBM surface" << std::endl;
        return false;
    }
    
    eglSurface_ = eglCreateWindowSurface(eglDisplay_, config, 
                                         (EGLNativeWindowType)gbmSurface_, nullptr);
    if (eglSurface_ == EGL_NO_SURFACE) {
        std::cerr << "Failed to create EGL surface" << std::endl;
        return false;
    }
    
    // 创建上下文
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    
    eglContext_ = eglCreateContext(eglDisplay_, config, EGL_NO_CONTEXT, contextAttribs);
    if (eglContext_ == EGL_NO_CONTEXT) {
        std::cerr << "Failed to create EGL context" << std::endl;
        return false;
    }
    
    if (!eglMakeCurrent(eglDisplay_, eglSurface_, eglSurface_, eglContext_)) {
        std::cerr << "Failed to make EGL context current" << std::endl;
        return false;
    }
    
    return true;
}
bool PlatformWindowDRM::SetupDumbBuffer() {
    struct drm_mode_create_dumb create = {};
    create.width = width_;
    create.height = height_;
    create.bpp = 32;
    
    if (drmIoctl(drmFd_, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
        std::cerr << "Failed to create dumb buffer" << std::endl;
        return false;
    }
    
    dumb_.handle = create.handle;
    dumb_.pitch = create.pitch;
    dumb_.size = create.size;
    
    struct drm_mode_map_dumb map = {};
    map.handle = dumb_.handle;
    if (drmIoctl(drmFd_, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
        std::cerr << "Failed to map dumb buffer" << std::endl;
        return false;
    }
    
    dumb_.map = mmap(nullptr, dumb_.size, PROT_READ | PROT_WRITE, MAP_SHARED, drmFd_, map.offset);
    if (dumb_.map == MAP_FAILED) {
        std::cerr << "Failed to mmap dumb buffer" << std::endl;
        dumb_.map = nullptr;
        return false;
    }
    
    // 清空缓冲区
    memset(dumb_.map, 0, dumb_.size);
    
    return true;
}
void PlatformWindowDRM::InputThreadFunc() {
    struct input_event ev;
    
    while (running_) {
        ssize_t bytes = read(evdevFd_, &ev, sizeof(ev));
        if (bytes == sizeof(ev)) {
            ProcessEvdevEvent(ev);
        } else if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            break;
        }
        
        // 短暂休眠避免CPU占用过高
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
void PlatformWindowDRM::ProcessEvdevEvent(const struct input_event& ev) {
    if (!callback_) return;
    
    Event event;
    
    switch (ev.type) {
        case EV_KEY:
            if (ev.code >= BTN_LEFT && ev.code <= BTN_TASK) {
                // 鼠标按钮
                event.type = ev.value ? Event::Type::MouseDown : Event::Type::MouseUp;
                event.button = MapEvdevButton(ev.code);
            } else {
                // 键盘按键
                event.type = ev.value ? Event::Type::KeyDown : Event::Type::KeyUp;
                event.keyCode = MapEvdevKey(ev.code);
            }
            break;
            
        case EV_REL:
            if (ev.code == REL_X) {
                event.type = Event::Type::MouseMove;
                event.x += ev.value;
            } else if (ev.code == REL_Y) {
                event.type = Event::Type::MouseMove;
                event.y += ev.value;
            } else if (ev.code == REL_WHEEL) {
                event.type = Event::Type::MouseWheel;
                event.wheelDelta = static_cast<float>(ev.value);
            }
            break;
            
        case EV_ABS:
            // 触摸屏事件（简化处理）
            if (ev.code == ABS_X || ev.code == ABS_MT_POSITION_X) {
                event.type = Event::Type::TouchMove;
                event.x = ev.value;
            } else if (ev.code == ABS_Y || ev.code == ABS_MT_POSITION_Y) {
                event.type = Event::Type::TouchMove;
                event.y = ev.value;
            }
            break;
    }
    
    if (event.type != Event::Type::None) {
        std::lock_guard<std::mutex> lock(inputMutex_);
        callback_(event);
    }
}
Event::MouseButton PlatformWindowDRM::MapEvdevButton(uint32_t code) {
    switch (code) {
        case BTN_LEFT: return Event::MouseButton::Left;
        case BTN_RIGHT: return Event::MouseButton::Right;
        case BTN_MIDDLE: return Event::MouseButton::Middle;
        default: return Event::MouseButton::None;
    }
}
uint32_t PlatformWindowDRM::MapEvdevKey(uint32_t code) {
    // 简化映射，实际需要完整的键码映射表
    return code;
}
} // namespace kwik::platform