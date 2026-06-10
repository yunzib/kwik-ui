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
#include <vector>
#include <cstdint>

export module kwik.render.vulkan.context;
import kwik.core.types;

import std;

export struct GlyphPushConstants {
    float posX, posY;
    float sizeX, sizeY;
    float uvU0, uvV0;
    float uvU1, uvV1;
    float colorR, colorG, colorB, colorA;
    float viewportW, viewportH;
    float cornerRadius;
};
static_assert(sizeof(GlyphPushConstants) == 60, "GlyphPushConstants size must match shader layout");

// ── FrameToken: 每帧由 beginFrame 产出，子渲染器通过此对象获取 Vulkan 句柄 ──
export struct FrameToken {
    VkCommandBuffer commandBuffer;
    VkExtent2D extent;
    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;
};

// ── DeviceContext: 子渲染器 upload 操作所需的最小设备句柄集 ──
export struct DeviceContext {
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkCommandPool commandPool;
    VkQueue queue;
};

export class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext();

    // ── 生命周期 ──
    bool initialize(void *nativeHandle, int width, int height);
    void shutdown();
    bool resize(int width, int height);    // 返回 bool

    // ── 帧控制 ──
    std::optional<FrameToken> beginFrame();
    void endFrame();
    bool present();

    // ── 资源访问（仅子渲染器 setup / upload 时用） ──
    VkDevice device() const;
    VkQueue graphicsQueue() const;
    VkCommandPool commandPool() const;
    VkRenderPass renderPass() const;
    VkPhysicalDevice physicalDevice() const;
    VkBuffer vertexBuffer() const;    // 仅初始化时用
    VkBuffer indexBuffer() const;     // 仅初始化时用

    // ── 工具 ──
    static uint32_t findMemoryType(VkPhysicalDevice physDevice, uint32_t typeFilter, VkMemoryPropertyFlags props);
    static bool createBuffer(VkDevice device, VkPhysicalDevice physDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags props, VkBuffer &buffer, VkDeviceMemory &memory);
    bool copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
    static VkShaderModule createShaderModule(VkDevice device, const uint8_t *spv, size_t size);
    // ── 便捷版（使用内部 device/physicalDevice） ──
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer &buffer,
                      VkDeviceMemory &memory);

private:
    VkFormat depthStencilFormat_ = VK_FORMAT_D24_UNORM_S8_UINT;
    VkInstance vkInstance_ = VK_NULL_HANDLE;
    VkSurfaceKHR vkSurface_ = VK_NULL_HANDLE;
    VkPhysicalDevice vkPhysicalDevice_ = VK_NULL_HANDLE;
    VkDevice vkDevice_ = VK_NULL_HANDLE;
    VkQueue vkQueue_ = VK_NULL_HANDLE;
    uint32_t queueFamilyIndex_ = 0;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_R8G8B8A8_SRGB;
    VkExtent2D swapchainExtent_ = {800, 600};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory_ = VK_NULL_HANDLE;
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    uint32_t frameIndex_ = 0;
    uint32_t currentImageIndex_ = 0;
    VkImage canvasImage_ = VK_NULL_HANDLE;
    VkDeviceMemory canvasMemory_ = VK_NULL_HANDLE;
    VkImageView canvasView_ = VK_NULL_HANDLE;
    VkImage canvasStencilImage_ = VK_NULL_HANDLE;
    VkDeviceMemory canvasStencilMemory_ = VK_NULL_HANDLE;
    VkImageView canvasStencilView_ = VK_NULL_HANDLE;
    VkFramebuffer canvasFramebuffer_ = VK_NULL_HANDLE;

    bool createInstance(void *nativeHandle);
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    bool createCanvasImage();
    void destroyCanvas();
    bool createSwapchain();
    void cleanupSwapchain();
    bool createRenderPass();
    bool createCanvasFramebuffer();
    bool createVertexBuffer();
    bool createCommandBuffers();
    bool createSyncObjects();
#if defined(__linux__)
    Display *x11Display_ = nullptr;
#endif
};