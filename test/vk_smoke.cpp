// vk_smoke.cpp — 最小 Vulkan 冒烟测试：测驱动/ICD 逐进程内存地板
// 用法: vk_smoke [width] [height]   默认 1920x1080
// 无任何渲染/无 kwik 代码, 只建 instance/device/surface/swapchain 后空转
#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <vector>

static VkInstance g_instance = VK_NULL_HANDLE;
static VkSurfaceKHR g_surface = VK_NULL_HANDLE;
static VkDevice g_device = VK_NULL_HANDLE;
static VkSwapchainKHR g_swapchain = VK_NULL_HANDLE;
static VkQueue g_queue = VK_NULL_HANDLE;
static uint32_t g_queueFamily = 0;

static bool ok(VkResult r, const char *what) {
    if (r == VK_SUCCESS) return true;
    std::printf("FAILED %s: %d\n", what, (int)r);
    return false;
}

static bool createInstance() {
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "vk_smoke";
    app.applicationVersion = 1;
    app.apiVersion = VK_API_VERSION_1_0;

    std::vector<const char *> ext = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = (uint32_t)ext.size();
    ci.ppEnabledExtensionNames = ext.data();
    return ok(vkCreateInstance(&ci, nullptr, &g_instance), "vkCreateInstance");
}

static bool createWindowAndSurface(HWND &hwnd, uint32_t w, uint32_t h) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = [](HWND h, UINT m, WPARAM, LPARAM) -> LRESULT {
        return m == WM_DESTROY ? (PostQuitMessage(0), 0L) : DefWindowProcW(h, m, 0, 0);
    };
    wc.lpszClassName = L"vk_smoke";
    wc.hInstance = GetModuleHandleW(nullptr);
    RegisterClassW(&wc);

    RECT r{0, 0, (LONG)w, (LONG)h};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    hwnd = CreateWindowW(wc.lpszClassName, L"vk_smoke", WS_OVERLAPPEDWINDOW, 100, 100, r.right - r.left,
                         r.bottom - r.top, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    VkWin32SurfaceCreateInfoKHR si{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    si.hinstance = GetModuleHandleW(nullptr);
    si.hwnd = hwnd;
    return ok(vkCreateWin32SurfaceKHR(g_instance, &si, nullptr, &g_surface), "vkCreateWin32SurfaceKHR");
}

static bool createDeviceAndSwapchain(HWND hwnd, uint32_t w, uint32_t h) {
    uint32_t gpuCount = 0;
    if (!ok(vkEnumeratePhysicalDevices(g_instance, &gpuCount, nullptr), "enum gpus")) return false;
    std::vector<VkPhysicalDevice> gpus(gpuCount);
    vkEnumeratePhysicalDevices(g_instance, &gpuCount, gpus.data());
    VkPhysicalDevice gpu = gpus[0];

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(gpu, &props);
    std::printf("GPU: %s\n", props.deviceName);

    uint32_t qfamCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &qfamCount, nullptr);
    std::vector<VkQueueFamilyProperties> qfam(qfamCount);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &qfamCount, qfam.data());
    g_queueFamily = 0;
    for (uint32_t i = 0; i < qfamCount; i++)
        if (qfam[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            g_queueFamily = i;
            break;
        }

    const char *devExt = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = g_queueFamily;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = &devExt;
    if (!ok(vkCreateDevice(gpu, &dci, nullptr, &g_device), "vkCreateDevice")) return false;
    vkGetDeviceQueue(g_device, g_queueFamily, 0, &g_queue);

    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, g_surface, &caps);
    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, g_surface, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> fmts(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, g_surface, &fmtCount, fmts.data());
    VkSurfaceFormatKHR fmt =
        fmts.empty() ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR} : fmts[0];

    VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface = g_surface;
    sci.minImageCount = 2;
    sci.imageFormat = fmt.format;
    sci.imageColorSpace = fmt.colorSpace;
    sci.imageExtent = {w, h};
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    return ok(vkCreateSwapchainKHR(g_device, &sci, nullptr, &g_swapchain), "vkCreateSwapchainKHR");
}

static void presentAFewFrames() {
    // 让驱动把呈现路径/命令分配器激活, 再回到空闲
    VkCommandPool pool;
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, 0, g_queueFamily};
    if (vkCreateCommandPool(g_device, &pci, nullptr, &pool) != VK_SUCCESS) return;
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, pool,
                                    VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
    VkCommandBuffer cb;
    vkAllocateCommandBuffers(g_device, &cai, &cb);

    uint32_t imgCount = 0;
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &imgCount, nullptr);
    std::vector<VkImage> images(imgCount);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &imgCount, images.data());

    for (int i = 0; i < 3; i++) {
        uint32_t idx = 0;
        if (vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX, VK_NULL_HANDLE, VK_NULL_HANDLE, &idx)
            != VK_SUCCESS)
            break;
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(cb, &bi);
        vkEndCommandBuffer(cb);
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cb;
        vkQueueSubmit(g_queue, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(g_queue);
        VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        pi.swapchainCount = 1;
        pi.pSwapchains = &g_swapchain;
        pi.pImageIndices = &idx;
        vkQueuePresentKHR(g_queue, &pi);
        vkQueueWaitIdle(g_queue);
    }
    vkDestroyCommandPool(g_device, pool, nullptr);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    uint32_t w = 1920, h = 1080;
    // 也可用命令行: vk_smoke 2880 1800
    if (__argc >= 3) {
        w = (uint32_t)std::atoi(__argv[1]);
        h = (uint32_t)std::atoi(__argv[2]);
    }

    if (!createInstance()) return 1;
    HWND hwnd = nullptr;
    if (!createWindowAndSurface(hwnd, w, h)) return 1;
    if (!createDeviceAndSwapchain(hwnd, w, h)) return 1;
    presentAFewFrames();

    std::printf(">>> ready: GPU 初始化完毕, 空转中. 在任务管理器查看内存. 关闭窗口退出.\n");

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        Sleep(1);    // 空转, 模拟空闲帧循环
    }
    return 0;
}

// 2. 编译（llvm-mingw，需 Vulkan SDK 环境变量 VULKAN_SDK）
// clang++ vk_smoke.cpp -O2 -std=c++17 -o vk_smoke.exe -I"%VULKAN_SDK%\Include" -L"%VULKAN_SDK%\Lib" -lvulkan-1 -luser32
// -lgdi32 -static
// 3. 运行 + 读数
// vk_smoke.exe 1920 1080      # 对应你的 1K
// vk_smoke.exe 2880 1800      # 对应你的 2K

// 实际测试：./ vk_smoke.exe 2880 1800 41.4 ./ vk_smoke.exe 41.2

// 1K（1920×1080，共 62M）
// 成分	                            大小
// Vulkan 驱动地板（冒烟实测）	        41.0M
// 画布 RGBA	                        7.9M
// 深度-模板 D24S8	                    7.9M
// 应用自身（JS+字体+View 树+代码）	≈ 5.2M

// 2K（2880×1800，共 93M）
// 成分	大小
// Vulkan 驱动地板（冒烟实测）	        41.0M
// 画布 RGBA	                        19.8M
// 深度-模板 D24S8	                    19.8M
// 应用自身（反推）	                ≈ 12.4M