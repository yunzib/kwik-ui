module;
#include <vulkan/vulkan.h>
#if defined(_WIN32)
#include <Windows.h>
#include <vulkan/vulkan_win32.h>
#elif defined(__linux__)
#include <vulkan/vulkan_xlib.h>
#include <vulkan/vulkan_wayland.h>
#include <X11/Xlib.h>
#endif

module kwik.render.vulkan_backend;

import std;
import kwik.core.types;

VulkanBackend::VulkanBackend() = default;

VulkanBackend::VulkanBackend(int width, int height) : width_(width), height_(height) {
}

VulkanBackend::~VulkanBackend() {
    shutdown();
}

bool VulkanBackend::initialize(void *nativeHandle, int width, int height) {
    nativeHandle_ = nativeHandle;
    width_ = width;
    height_ = height;
    return initVulkan(nativeHandle, width, height);
}

void VulkanBackend::shutdown() {
    cleanupVulkan();
}

void VulkanBackend::resize(int width, int height) {
    if (width_ == width && height_ == height) { return; }
    width_ = width;
    height_ = height;
    // 需要重新创建交换链等，暂不实现
}

bool VulkanBackend::beginFrame() {
    // 开始命令缓冲区录制
    return true;
}

void VulkanBackend::endFrame() {
    // 提交命令缓冲区
}

void VulkanBackend::present() {
    // 交换链呈现
}

void VulkanBackend::setGlobalAlpha(float alpha) {
    globalAlpha_ = std::clamp(alpha, 0.0f, 1.0f);
}

void VulkanBackend::pushClipRoundedRect(const Rect &rect, float radius) {
    // 暂不实现裁剪
}

void VulkanBackend::resetClip() {
    // 暂不实现裁剪
}

void VulkanBackend::clear(const Color &color) {
    // 清除颜色附件
}

void VulkanBackend::fillRect(const Rect &rect, const Color &color) {
    // 暂不实现
}

void VulkanBackend::fillRoundedRect(const Rect &rect, float radius, const Color &color) {
    // 暂不实现
}

void VulkanBackend::strokeRoundedRect(const Rect &rect, float radius, const Color &color, float strokeWidth) {
    // 暂不实现
}

void VulkanBackend::drawShadow(const Rect &rect, float radius, const Shadow &shadow) {
    // 暂不实现
}

bool VulkanBackend::initVulkan(void *nativeHandle, int width, int height) {
    // 1. 创建Vulkan Instance
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "KwiK UI";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "KwiK";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    std::vector<const char *> extensions;
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

#if defined(_WIN32)
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
    extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (vkCreateInstance(&createInfo, nullptr, &vkInstance_) != VK_SUCCESS) { return false; }

    // 2. 创建Surface
#if defined(_WIN32)
    VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hinstance = GetModuleHandle(nullptr);
    surfaceInfo.hwnd = static_cast<HWND>(nativeHandle);

    if (vkCreateWin32SurfaceKHR(vkInstance_, &surfaceInfo, nullptr, &vkSurface_) != VK_SUCCESS) {
        vkDestroyInstance(vkInstance_, nullptr);
        vkInstance_ = VK_NULL_HANDLE;
        return false;
    }
#elif defined(__linux__)
    VkXlibSurfaceCreateInfoKHR surfaceInfo = {};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.dpy = XOpenDisplay(nullptr);
    surfaceInfo.window = reinterpret_cast<Window>(nativeHandle);

    if (vkCreateXlibSurfaceKHR(vkInstance_, &surfaceInfo, nullptr, &vkSurface_) != VK_SUCCESS) {
        vkDestroyInstance(vkInstance_, nullptr);
        vkInstance_ = VK_NULL_HANDLE;
        return false;
    }
    x11Display_ = surfaceInfo.dpy;
#endif

    // 3. 选择物理设备
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(vkInstance_, &deviceCount, nullptr);
    if (deviceCount == 0) {
        cleanupVulkan();
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(vkInstance_, &deviceCount, devices.data());
    vkPhysicalDevice_ = devices[0];

    // 4. 查找队列家族
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_, &queueFamilyCount, queueFamilies.data());

    uint32_t graphicsQueueFamily = 0;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevice_, i, vkSurface_, &presentSupport);
            if (presentSupport) {
                graphicsQueueFamily = i;
                break;
            }
        }
    }

    // 5. 创建逻辑设备
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = graphicsQueueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    const char *deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.ppEnabledExtensionNames = deviceExtensions;

    if (vkCreateDevice(vkPhysicalDevice_, &deviceInfo, nullptr, &vkDevice_) != VK_SUCCESS) {
        cleanupVulkan();
        return false;
    }

    // 6. 获取队列
    vkGetDeviceQueue(vkDevice_, graphicsQueueFamily, 0, &vkQueue_);

    // 7. 创建渲染管线等（暂不实现）

    width_ = width;
    height_ = height;

    return true;
}

void VulkanBackend::cleanupVulkan() {
    // 清理Vulkan资源
    if (vkDevice_ != VK_NULL_HANDLE) {
        vkDestroyDevice(vkDevice_, nullptr);
        vkDevice_ = VK_NULL_HANDLE;
    }

    if (vkSurface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(vkInstance_, vkSurface_, nullptr);
        vkSurface_ = VK_NULL_HANDLE;
    }

    if (vkInstance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(vkInstance_, nullptr);
        vkInstance_ = VK_NULL_HANDLE;
    }

#if defined(__linux__)
    if (x11Display_) {
        XCloseDisplay(x11Display_);
        x11Display_ = nullptr;
    }
#endif
}
