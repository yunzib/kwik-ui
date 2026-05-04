module;

#if defined(__ANDROID__)
#include <android/native_activity.h>
#include <android/input.h>
#include <android/native_window.h>
#include <android/looper.h>
#include <pthread.h>
#include <cstring>
#include <mutex>
#include <condition_variable>
export module kwik.platform.android_window;
import kwik.platform.platform_window;
import kwik.core.types;
import std;
export namespace kwik::platform {
    /**
    * @brief Android平台窗口实现
    * 
    * 使用Android NativeActivity进行原生窗口渲染
    * 支持触摸、键盘输入和Android生命周期管理
    */
    class PlatformWindowAndroid : public PlatformWindow {
    public:
        PlatformWindowAndroid();
        ~PlatformWindowAndroid() override;
        
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
        
        // Android特定方法
        void SetAndroidApp(android_app* app);
        static void HandleCmd(android_app* app, int32_t cmd);
        static int32_t HandleInput(android_app* app, AInputEvent* event);
        
    private:
        // Android相关
        android_app* androidApp_ = nullptr;
        ANativeWindow* nativeWindow_ = nullptr;
        ANativeWindow_Buffer windowBuffer_;
        
        // 状态管理
        bool windowReady_ = false;
        bool hasFocus_ = false;
        bool windowVisible_ = false;
        int width_ = 0;
        int height_ = 0;
        int32_t format_ = WINDOW_FORMAT_RGBA_8888;
        
        // 事件处理
        EventCallback callback_;
        std::mutex eventMutex_;
        std::condition_variable eventCondition_;
        
        // 渲染状态
        bool bufferLocked_ = false;
        void* lockedBuffer_ = nullptr;
        int lockedStride_ = 0;
        
        // 辅助方法
        void ProcessCommand(int32_t cmd);
        void ProcessInputEvent(AInputEvent* event);
        void UpdateWindowSize();
        static Event::MouseButton MapAndroidButton(int32_t button);
        static uint32_t MapAndroidKey(int32_t keyCode);
    };
}
#endif // __ANDROID__