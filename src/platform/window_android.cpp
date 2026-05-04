module kwik.platform.android_window;
#include <iostream>
#include <chrono>
namespace kwik::platform {
// 全局应用映射
static std::map<android_app*, PlatformWindowAndroid*> g_appMap;
static std::mutex g_appMapMutex;
PlatformWindowAndroid::PlatformWindowAndroid() = default;
PlatformWindowAndroid::~PlatformWindowAndroid() {
    Destroy();
}
bool PlatformWindowAndroid::Create(const std::string& title, int width, int height) {
    if (!androidApp_) {
        std::cerr << "Android app not set. Call SetAndroidApp() first." << std::endl;
        return false;
    }
    
    // 注册到全局映射
    {
        std::lock_guard<std::mutex> lock(g_appMapMutex);
        g_appMap[androidApp_] = this;
    }
    
    // 设置回调
    androidApp_->onAppCmd = HandleCmd;
    androidApp_->onInputEvent = HandleInput;
    
    // 等待窗口创建
    int attempts = 0;
    while (!windowReady_ && attempts < 100) {
        PollEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        attempts++;
    }
    
    if (!windowReady_) {
        std::cerr << "Timeout waiting for Android window creation" << std::endl;
        return false;
    }
    
    UpdateWindowSize();
    return true;
}
void PlatformWindowAndroid::Destroy() {
    if (androidApp_) {
        std::lock_guard<std::mutex> lock(g_appMapMutex);
        g_appMap.erase(androidApp_);
    }
    
    windowReady_ = false;
    hasFocus_ = false;
    windowVisible_ = false;
    nativeWindow_ = nullptr;
    callback_ = nullptr;
}
void PlatformWindowAndroid::Show() {
    windowVisible_ = true;
    // Android窗口由系统管理，这里只是更新状态
}
void PlatformWindowAndroid::Hide() {
    windowVisible_ = false;
    // Android窗口由系统管理，这里只是更新状态
}
void PlatformWindowAndroid::GetSize(int* width, int* height) const {
    if (width) *width = width_;
    if (height) *height = height_;
}
bool PlatformWindowAndroid::LockBackBuffer(void** pixels, int* stride) {
    if (!nativeWindow_ || bufferLocked_) {
        return false;
    }
    
    int result = ANativeWindow_lock(nativeWindow_, &windowBuffer_, nullptr);
    if (result < 0) {
        std::cerr << "Failed to lock Android window buffer: " << result << std::endl;
        return false;
    }
    
    bufferLocked_ = true;
    lockedBuffer_ = windowBuffer_.bits;
    lockedStride_ = windowBuffer_.stride;
    
    if (pixels) *pixels = lockedBuffer_;
    if (stride) *stride = lockedStride_ * (windowBuffer_.format == AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM ? 4 : 2);
    
    return true;
}
void PlatformWindowAndroid::UnlockBackBuffer() {
    if (bufferLocked_ && nativeWindow_) {
        ANativeWindow_unlockAndPost(nativeWindow_);
        bufferLocked_ = false;
        lockedBuffer_ = nullptr;
        lockedStride_ = 0;
    }
}
void PlatformWindowAndroid::Present() {
    // 在Android上，Present在UnlockBackBuffer中自动处理
    // 这里无需额外操作
}
void* PlatformWindowAndroid::GetNativeHandle() const {
    return static_cast<void*>(nativeWindow_);
}
void PlatformWindowAndroid::SetEventCallback(EventCallback callback) {
    callback_ = std::move(callback);
}
void PlatformWindowAndroid::PollEvents() {
    if (!androidApp_) return;
    
    int ident;
    int events;
    struct android_poll_source* source;
    
    // 非阻塞轮询
    while ((ident = ALooper_pollAll(0, nullptr, &events, (void**)&source)) >= 0) {
        if (source) {
            source->process(androidApp_, source);
        }
        
        // 检查是否要退出
        if (androidApp_->destroyRequested) {
            break;
        }
    }
}
void PlatformWindowAndroid::WaitEvents() {
    if (!androidApp_) return;
    
    int ident;
    int events;
    struct android_poll_source* source;
    
    // 阻塞等待事件
    while ((ident = ALooper_pollAll(-1, nullptr, &events, (void**)&source)) >= 0) {
        if (source) {
            source->process(androidApp_, source);
        }
        
        // 检查是否要退出
        if (androidApp_->destroyRequested) {
            break;
        }
        
        // 有事件到达，退出等待
        break;
    }
}
void PlatformWindowAndroid::SetDecoration(WindowDecoration decoration) {
    // Android不支持窗口装饰
}
void PlatformWindowAndroid::SetShape(const std::vector<std::pair<int, int>>& polygon) {
    // Android不支持异形窗口
}
void PlatformWindowAndroid::SetShapeMask(const uint8_t* maskData, int width, int height) {
    // Android不支持异形窗口
}
void PlatformWindowAndroid::SetResizable(bool resizable) {
    // Android窗口大小由系统控制
}
void PlatformWindowAndroid::SetAndroidApp(android_app* app) {
    androidApp_ = app;
}
// ==================== 静态回调方法 ====================
void PlatformWindowAndroid::HandleCmd(android_app* app, int32_t cmd) {
    PlatformWindowAndroid* window = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_appMapMutex);
        auto it = g_appMap.find(app);
        if (it != g_appMap.end()) {
            window = it->second;
        }
    }
    
    if (window) {
        window->ProcessCommand(cmd);
    }
}
int32_t PlatformWindowAndroid::HandleInput(android_app* app, AInputEvent* event) {
    PlatformWindowAndroid* window = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_appMapMutex);
        auto it = g_appMap.find(app);
        if (it != g_appMap.end()) {
            window = it->second;
        }
    }
    
    if (window) {
        window->ProcessInputEvent(event);
        return 1; // 事件已处理
    }
    
    return 0; // 事件未处理
}
// ==================== 私有方法实现 ====================
void PlatformWindowAndroid::ProcessCommand(int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            nativeWindow_ = androidApp_->window;
            windowReady_ = true;
            UpdateWindowSize();
            
            // 发送窗口创建事件
            if (callback_) {
                Event event;
                event.type = Event::Type::WindowResize;
                event.width = width_;
                event.height = height_;
                callback_(event);
            }
            break;
            
        case APP_CMD_TERM_WINDOW:
            windowReady_ = false;
            nativeWindow_ = nullptr;
            break;
            
        case APP_CMD_GAINED_FOCUS:
            hasFocus_ = true;
            break;
            
        case APP_CMD_LOST_FOCUS:
            hasFocus_ = false;
            break;
            
        case APP_CMD_WINDOW_RESIZED:
            UpdateWindowSize();
            
            if (callback_) {
                Event event;
                event.type = Event::Type::WindowResize;
                event.width = width_;
                event.height = height_;
                callback_(event);
            }
            break;
            
        case APP_CMD_CONFIG_CHANGED:
            UpdateWindowSize();
            break;
            
        case APP_CMD_SAVE_STATE:
            // 保存应用状态
            break;
            
        case APP_CMD_DESTROY:
            // 应用销毁
            if (callback_) {
                Event event;
                event.type = Event::Type::WindowClose;
                callback_(event);
            }
            break;
    }
}
void PlatformWindowAndroid::ProcessInputEvent(AInputEvent* event) {
    if (!callback_) return;
    
    Event uiEvent;
    int32_t eventType = AInputEvent_getType(event);
    
    if (eventType == AINPUT_EVENT_TYPE_MOTION) {
        // 触摸事件
        int32_t action = AMotionEvent_getAction(event);
        int32_t actionMasked = action & AMOTION_EVENT_ACTION_MASK;
        int32_t pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) 
                               >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        
        float x = AMotionEvent_getX(event, pointerIndex);
        float y = AMotionEvent_getY(event, pointerIndex);
        
        switch (actionMasked) {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
                uiEvent.type = Event::Type::TouchBegin;
                break;
            case AMOTION_EVENT_ACTION_MOVE:
                uiEvent.type = Event::Type::TouchMove;
                break;
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
                uiEvent.type = Event::Type::TouchEnd;
                break;
            case AMOTION_EVENT_ACTION_CANCEL:
                uiEvent.type = Event::Type::TouchCancel;
                break;
        }
        
        uiEvent.x = static_cast<int>(x);
        uiEvent.y = static_cast<int>(y);
        uiEvent.touchId = AMotionEvent_getPointerId(event, pointerIndex);
        uiEvent.pressure = AMotionEvent_getPressure(event, pointerIndex);
        
    } else if (eventType == AINPUT_EVENT_TYPE_KEY) {
        // 键盘事件
        int32_t keyCode = AKeyEvent_getKeyCode(event);
        int32_t action = AKeyEvent_getAction(event);
        
        uiEvent.type = (action == AKEY_EVENT_ACTION_DOWN) 
                      ? Event::Type::KeyDown 
                      : Event::Type::KeyUp;
        uiEvent.keyCode = MapAndroidKey(keyCode);
        
        // 获取修饰键状态
        int32_t metaState = AKeyEvent_getMetaState(event);
        uiEvent.modifiers = 0;
        if (metaState & AMETA_CTRL_ON) uiEvent.modifiers |= 1;
        if (metaState & AMETA_SHIFT_ON) uiEvent.modifiers |= 2;
        if (metaState & AMETA_ALT_ON) uiEvent.modifiers |= 4;
        if (metaState & AMETA_META_ON) uiEvent.modifiers |= 8;
    }
    
    if (uiEvent.type != Event::Type::None) {
        std::lock_guard<std::mutex> lock(eventMutex_);
        callback_(uiEvent);
    }
}
void PlatformWindowAndroid::UpdateWindowSize() {
    if (nativeWindow_) {
        width_ = ANativeWindow_getWidth(nativeWindow_);
        height_ = ANativeWindow_getHeight(nativeWindow_);
        format_ = ANativeWindow_getFormat(nativeWindow_);
    }
}
Event::MouseButton PlatformWindowAndroid::MapAndroidButton(int32_t button) {
    switch (button) {
        case AMOTION_EVENT_BUTTON_PRIMARY: return Event::MouseButton::Left;
        case AMOTION_EVENT_BUTTON_SECONDARY: return Event::MouseButton::Right;
        case AMOTION_EVENT_BUTTON_TERTIARY: return Event::MouseButton::Middle;
        default: return Event::MouseButton::None;
    }
}
uint32_t PlatformWindowAndroid::MapAndroidKey(int32_t keyCode) {
    // Android键码到通用键码的映射
    static const std::map<int32_t, uint32_t> keyMap = {
        {AKEYCODE_ESCAPE, 27},
        {AKEYCODE_ENTER, 13},
        {AKEYCODE_SPACE, 32},
        {AKEYCODE_DEL, 8},
        {AKEYCODE_TAB, 9},
        {AKEYCODE_DPAD_LEFT, 37},
        {AKEYCODE_DPAD_RIGHT, 39},
        {AKEYCODE_DPAD_UP, 38},
        {AKEYCODE_DPAD_DOWN, 40},
        {AKEYCODE_MOVE_HOME, 36},
        {AKEYCODE_MOVE_END, 35},
        {AKEYCODE_PAGE_UP, 33},
        {AKEYCODE_PAGE_DOWN, 34},
        {AKEYCODE_FORWARD_DEL, 46},
        {AKEYCODE_INSERT, 45},
        {AKEYCODE_F1, 112},
        {AKEYCODE_F2, 113},
        {AKEYCODE_F3, 114},
        {AKEYCODE_F4, 115},
        {AKEYCODE_F5, 116},
        {AKEYCODE_F6, 117},
        {AKEYCODE_F7, 118},
        {AKEYCODE_F8, 119},
        {AKEYCODE_F9, 120},
        {AKEYCODE_F10, 121},
        {AKEYCODE_F11, 122},
        {AKEYCODE_F12, 123},
    };
    
    auto it = keyMap.find(keyCode);
    if (it != keyMap.end()) {
        return it->second;
    }
    
    // 字母键
    if (keyCode >= AKEYCODE_A && keyCode <= AKEYCODE_Z) {
        return 'A' + (keyCode - AKEYCODE_A);
    }
    
    // 数字键
    if (keyCode >= AKEYCODE_0 && keyCode <= AKEYCODE_9) {
        return '0' + (keyCode - AKEYCODE_0);
    }
    
    // 小键盘数字键
    if (keyCode >= AKEYCODE_NUMPAD_0 && keyCode <= AKEYCODE_NUMPAD_9) {
        return '0' + (keyCode - AKEYCODE_NUMPAD_0);
    }
    
    return static_cast<uint32_t>(keyCode);
}
} // namespace kwik::platform