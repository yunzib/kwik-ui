module kwik.platform.fbdev_window;
#include <iostream>
#include <poll.h>
#include <algorithm>
namespace kwik::platform {
PlatformWindowFBDev::PlatformWindowFBDev() = default;
PlatformWindowFBDev::~PlatformWindowFBDev() {
    Destroy();
}
bool PlatformWindowFBDev::Create(const std::string& title, int width, int height) {
    // 1. 打开帧缓冲设备
    if (!OpenFrameBuffer()) {
        return false;
    }
    
    // 2. 设置窗口尺寸
    width_ = vinfo_.xres;
    height_ = vinfo_.yres;
    bpp_ = vinfo_.bits_per_pixel;
    
    // 如果指定了尺寸且支持修改，则尝试设置
    if (width > 0 && height > 0 && (vinfo_.xres != width || vinfo_.yres != height)) {
        vinfo_.xres = width;
        vinfo_.yres = height;
        vinfo_.xres_virtual = width;
        vinfo_.yres_virtual = height;
        
        if (ioctl(fbFd_, FBIOPUT_VSCREENINFO, &vinfo_) == 0) {
            width_ = width;
            height_ = height;
        }
    }
    
    // 3. 映射帧缓冲内存
    fbSize_ = finfo_.smem_len;
    fbPtr_ = mmap(nullptr, fbSize_, PROT_READ | PROT_WRITE, MAP_SHARED, fbFd_, 0);
    if (fbPtr_ == MAP_FAILED) {
        std::cerr << "Failed to mmap framebuffer" << std::endl;
        close(fbFd_);
        return false;
    }
    
    // 4. 设置双缓冲（如果支持）
    useDoubleBuffer_ = SetupDoubleBuffer();
    
    // 5. 打开输入设备
    if (!OpenInputDevices()) {
        std::cerr << "Warning: No input devices found" << std::endl;
    }
    
    // 6. 启动输入线程
    if (!inputFds_.empty()) {
        running_ = true;
        inputThread_ = std::thread(&PlatformWindowFBDev::InputThreadFunc, this);
    }
    
    return true;
}
void PlatformWindowFBDev::Destroy() {
    running_ = false;
    if (inputThread_.joinable()) {
        inputThread_.join();
    }
    
    CloseInputDevices();
    
    // 释放双缓冲
    if (backBuffer_) {
        free(backBuffer_);
        backBuffer_ = nullptr;
    }
    
    // 取消内存映射
    if (fbPtr_ && fbPtr_ != MAP_FAILED) {
        munmap(fbPtr_, fbSize_);
        fbPtr_ = nullptr;
    }
    
    // 关闭帧缓冲设备
    if (fbFd_ >= 0) {
        close(fbFd_);
        fbFd_ = -1;
    }
    
    width_ = height_ = bpp_ = 0;
    callback_ = nullptr;
}
void PlatformWindowFBDev::Show() {
    // 帧缓冲总是显示，无需额外操作
}
void PlatformWindowFBDev::Hide() {
    // 清空屏幕
    if (fbPtr_ && fbPtr_ != MAP_FAILED) {
        memset(fbPtr_, 0, fbSize_);
    }
}
void PlatformWindowFBDev::GetSize(int* width, int* height) const {
    if (width) *width = width_;
    if (height) *height = height_;
}
bool PlatformWindowFBDev::LockBackBuffer(void** pixels, int* stride) {
    if (!fbPtr_ || fbPtr_ == MAP_FAILED) {
        return false;
    }
    
    if (useDoubleBuffer_ && backBuffer_) {
        // 使用双缓冲：渲染到后缓冲区
        if (pixels) *pixels = backBuffer_;
        if (stride) *stride = finfo_.line_length;
    } else {
        // 单缓冲：直接渲染到帧缓冲
        if (pixels) *pixels = fbPtr_;
        if (stride) *stride = finfo_.line_length;
    }
    
    return true;
}
void PlatformWindowFBDev::UnlockBackBuffer() {
    // 在Present时提交，这里无需操作
}
void PlatformWindowFBDev::Present() {
    if (useDoubleBuffer_ && backBuffer_) {
        // 复制后缓冲区到帧缓冲
        memcpy(fbPtr_, backBuffer_, fbSize_);
    }
    
    // 等待垂直同步（如果支持）
    int arg = 0;
    if (ioctl(fbFd_, FBIO_WAITFORVSYNC, &arg) == 0) {
        // VSync等待成功
    }
}
void* PlatformWindowFBDev::GetNativeHandle() const {
    // 帧缓冲设备没有原生窗口句柄
    return nullptr;
}
void PlatformWindowFBDev::SetEventCallback(EventCallback callback) {
    callback_ = std::move(callback);
}
void PlatformWindowFBDev::PollEvents() {
    // 输入事件在独立线程中处理，这里无需操作
}
void PlatformWindowFBDev::WaitEvents() {
    // 等待输入事件
    if (inputFds_.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return;
    }
    
    struct pollfd fds[32];
    int nfds = std::min(static_cast<int>(inputFds_.size()), 32);
    
    for (int i = 0; i < nfds; ++i) {
        fds[i].fd = inputFds_[i];
        fds[i].events = POLLIN;
    }
    
    poll(fds, nfds, -1);
}
void PlatformWindowFBDev::SetDecoration(WindowDecoration decoration) {
    decoration_ = decoration;
    // 帧缓冲不支持窗口装饰
}
void PlatformWindowFBDev::SetShape(const std::vector<std::pair<int, int>>& polygon) {
    // 帧缓冲不支持异形窗口
}
void PlatformWindowFBDev::SetShapeMask(const uint8_t* maskData, int width, int height) {
    // 帧缓冲不支持异形窗口
}
void PlatformWindowFBDev::SetResizable(bool resizable) {
    // 帧缓冲不支持调整窗口大小
}
// ==================== 私有方法实现 ====================
bool PlatformWindowFBDev::OpenFrameBuffer() {
    const char* fbDevices[] = {"/dev/fb0", "/dev/fb1", "/dev/fb2"};
    
    for (const char* device : fbDevices) {
        fbFd_ = open(device, O_RDWR);
        if (fbFd_ >= 0) {
            // 获取固定屏幕信息
            if (ioctl(fbFd_, FBIOGET_FSCREENINFO, &finfo_) == 0 &&
                ioctl(fbFd_, FBIOGET_VSCREENINFO, &vinfo_) == 0) {
                return true;
            }
            close(fbFd_);
            fbFd_ = -1;
        }
    }
    
    std::cerr << "Failed to open framebuffer device" << std::endl;
    return false;
}
bool PlatformWindowFBDev::SetupDoubleBuffer() {
    // 检查是否支持双缓冲
    if (vinfo_.yres_virtual > vinfo_.yres) {
        // 硬件支持双缓冲
        return true;
    }
    
    // 软件双缓冲
    backBuffer_ = malloc(fbSize_);
    if (!backBuffer_) {
        std::cerr << "Failed to allocate back buffer" << std::endl;
        return false;
    }
    
    memset(backBuffer_, 0, fbSize_);
    return true;
}
bool PlatformWindowFBDev::OpenInputDevices() {
    DIR* dir = opendir("/dev/input");
    if (!dir) {
        return false;
    }
    
    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (strncmp(ent->d_name, "event", 5) == 0) {
            std::string path = std::string("/dev/input/") + ent->d_name;
            int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
            if (fd >= 0) {
                // 检查设备类型
                unsigned long evbit = 0;
                if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), &evbit) >= 0) {
                    // 只接受键盘、鼠标、触摸屏设备
                    if (evbit & (1 << EV_KEY) || evbit & (1 << EV_REL) || evbit & (1 << EV_ABS)) {
                        inputFds_.push_back(fd);
                    } else {
                        close(fd);
                    }
                } else {
                    close(fd);
                }
            }
        }
    }
    closedir(dir);
    
    return !inputFds_.empty();
}
void PlatformWindowFBDev::CloseInputDevices() {
    for (int fd : inputFds_) {
        close(fd);
    }
    inputFds_.clear();
}
void PlatformWindowFBDev::InputThreadFunc() {
    struct pollfd fds[32];
    
    while (running_) {
        int nfds = std::min(static_cast<int>(inputFds_.size()), 32);
        
        for (int i = 0; i < nfds; ++i) {
            fds[i].fd = inputFds_[i];
            fds[i].events = POLLIN;
            fds[i].revents = 0;
        }
        
        int ret = poll(fds, nfds, 100); // 100ms超时
        if (ret > 0) {
            for (int i = 0; i < nfds; ++i) {
                if (fds[i].revents & POLLIN) {
                    ProcessInputEvent(fds[i].fd);
                }
            }
        }
    }
}
void PlatformWindowFBDev::ProcessInputEvent(int fd) {
    struct input_event ev;
    
    while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
        if (!callback_) continue;
        
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
                // 触摸屏事件
                if (ev.code == ABS_X || ev.code == ABS_MT_POSITION_X) {
                    event.type = Event::Type::TouchMove;
                    event.x = ev.value * width_ / vinfo_.xres; // 转换为屏幕坐标
                } else if (ev.code == ABS_Y || ev.code == ABS_MT_POSITION_Y) {
                    event.type = Event::Type::TouchMove;
                    event.y = ev.value * height_ / vinfo_.yres; // 转换为屏幕坐标
                } else if (ev.code == ABS_MT_TRACKING_ID) {
                    if (ev.value >= 0) {
                        event.type = Event::Type::TouchBegin;
                        event.touchId = ev.value;
                    } else {
                        event.type = Event::Type::TouchEnd;
                    }
                }
                break;
                
            case EV_SYN:
                // 同步事件，发送累积的事件
                if (event.type != Event::Type::None) {
                    std::lock_guard<std::mutex> lock(inputMutex_);
                    callback_(event);
                }
                break;
        }
    }
}
void PlatformWindowFBDev::ConvertColorFormat(void* src, void* dst, int width, int height) {
    // 根据像素格式进行颜色转换
    // 这里实现ARGB8888到当前帧缓冲格式的转换
    // 简化实现：假设格式相同
    if (bpp_ == 32) {
        memcpy(dst, src, width * height * 4);
    } else if (bpp_ == 16) {
        // RGB565转换
        uint32_t* src32 = static_cast<uint32_t*>(src);
        uint16_t* dst16 = static_cast<uint16_t*>(dst);
        
        for (int i = 0; i < width * height; ++i) {
            uint32_t pixel = src32[i];
            uint8_t r = (pixel >> 16) & 0xFF;
            uint8_t g = (pixel >> 8) & 0xFF;
            uint8_t b = pixel & 0xFF;
            
            // RGB565: 5位红，6位绿，5位蓝
            dst16[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        }
    }
    // 其他格式可以继续扩展
}
Event::MouseButton PlatformWindowFBDev::MapEvdevButton(uint32_t code) {
    switch (code) {
        case BTN_LEFT: return Event::MouseButton::Left;
        case BTN_RIGHT: return Event::MouseButton::Right;
        case BTN_MIDDLE: return Event::MouseButton::Middle;
        default: return Event::MouseButton::None;
    }
}
uint32_t PlatformWindowFBDev::MapEvdevKey(uint32_t code) {
    // 简化键码映射
    static const std::map<uint32_t, uint32_t> keyMap = {
        {KEY_ESC, 27},
        {KEY_ENTER, 13},
        {KEY_SPACE, 32},
        {KEY_BACKSPACE, 8},
        {KEY_TAB, 9},
        {KEY_LEFT, 37},
        {KEY_RIGHT, 39},
        {KEY_UP, 38},
        {KEY_DOWN, 40},
        {KEY_HOME, 36},
        {KEY_END, 35},
        {KEY_PAGEUP, 33},
        {KEY_PAGEDOWN, 34},
        {KEY_DELETE, 46},
        {KEY_INSERT, 45},
        {KEY_F1, 112},
        {KEY_F2, 113},
        {KEY_F3, 114},
        {KEY_F4, 115},
        {KEY_F5, 116},
        {KEY_F6, 117},
        {KEY_F7, 118},
        {KEY_F8, 119},
        {KEY_F9, 120},
        {KEY_F10, 121},
        {KEY_F11, 122},
        {KEY_F12, 123},
    };
    
    auto it = keyMap.find(code);
    if (it != keyMap.end()) {
        return it->second;
    }
    
    // 字母和数字键
    if (code >= KEY_A && code <= KEY_Z) {
        return 'A' + (code - KEY_A);
    }
    if (code >= KEY_0 && code <= KEY_9) {
        return '0' + (code - KEY_0);
    }
    
    return code;
}
} // namespace kwik::platform