module;
#if defined(__linux__) && defined(USE_FBDEV)
#include <linux/fb.h>
#include <linux/input.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <cstring>
#include <thread>
#include <mutex>
#include <vector>
export module kwik.platform.fbdev_window;
import kwik.platform.window;
import kwik.core.types;
import std;
export namespace kwik::platform {
    /**
    * @brief Linux帧缓冲设备平台窗口实现
    * 
    * 使用Linux帧缓冲设备（/dev/fb0）进行直接渲染
    * 适用于传统嵌入式系统和无GPU的环境
    */
    class PlatformWindowFBDev : public PlatformWindow {
    public:
        PlatformWindowFBDev();
        ~PlatformWindowFBDev() override;
        
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
        // 帧缓冲相关
        int fbFd_ = -1;
        struct fb_fix_screeninfo finfo_;
        struct fb_var_screeninfo vinfo_;
        void* fbPtr_ = nullptr;
        size_t fbSize_ = 0;
        
        // 双缓冲支持
        void* backBuffer_ = nullptr;
        bool useDoubleBuffer_ = false;
        
        // 输入处理
        std::vector<int> inputFds_;
        std::thread inputThread_;
        std::mutex inputMutex_;
        bool running_ = false;
        void InputThreadFunc();
        void ProcessInputEvent(int fd);
        
        // 窗口状态
        int width_ = 0;
        int height_ = 0;
        int bpp_ = 0;
        EventCallback callback_;
        WindowDecoration decoration_ = WindowDecoration::Normal;
        
        // 辅助方法
        bool OpenFrameBuffer();
        bool SetupDoubleBuffer();
        bool OpenInputDevices();
        void CloseInputDevices();
        void ConvertColorFormat(void* src, void* dst, int width, int height);
        static Event::MouseButton MapEvdevButton(uint32_t code);
        static uint32_t MapEvdevKey(uint32_t code);
    };
}
#endif // __linux__ && USE_FBDEV