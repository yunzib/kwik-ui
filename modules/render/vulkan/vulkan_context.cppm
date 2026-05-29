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
/**
 * @brief Vulkan 核心资源管理器
 *
 * 管理设备、交换链、渲染通道、命令缓冲、同步对象等共享资源。
 * 不包含任何绘制逻辑，仅供子模块通过访问器获取句柄。
 */
export class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext();
    // 生命周期
    bool initialize(void *nativeHandle, int width, int height);
    void shutdown();
    bool resize(int width, int height);
    // 帧控制 — beginFrame 返回 false 时调用方应跳过本帧
    bool beginFrame();
    void endFrame();
    void present();
    // 访问器
    VkDevice device() const {
        return vkDevice_;
    }
    VkPhysicalDevice physicalDevice() const {
        return vkPhysicalDevice_;
    }
    VkQueue graphicsQueue() const {
        return vkQueue_;
    }
    VkRenderPass renderPass() const {
        return renderPass_;
    }
    VkCommandBuffer commandBuffer() const {
        return commandBuffers_[currentImageIndex_];
    }
    VkExtent2D extent() const {
        return swapchainExtent_;
    }
    VkSampleCountFlagBits msaaSamples() const {
        return msaaSamples_;
    }
    VkFormat swapchainFormat() const {
        return swapchainFormat_;
    }
    VkPipelineLayout pipelineLayout() const {
        return pipelineLayout_;
    }
    VkBuffer vertexBuffer() const {
        return vertexBuffer_;
    }
    VkBuffer indexBuffer() const {
        return indexBuffer_;
    }
    VkCommandPool commandPool() const {
        return commandPool_;
    }
    uint32_t queueFamily() const {
        return queueFamilyIndex_;
    }
    // 工具
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props);
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer &buffer,
                      VkDeviceMemory &memory);
    bool copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
    static VkShaderModule createShaderModule(VkDevice device, const std::uint8_t *spv, std::size_t size);

private:
    int width_ = 0, height_ = 0;
    VkSampleCountFlagBits msaaSamples_ = VK_SAMPLE_COUNT_4_BIT;
    VkFormat depthStencilFormat_ = VK_FORMAT_D24_UNORM_S8_UINT;
    // 核心
    VkInstance vkInstance_ = VK_NULL_HANDLE;
    VkSurfaceKHR vkSurface_ = VK_NULL_HANDLE;
    VkPhysicalDevice vkPhysicalDevice_ = VK_NULL_HANDLE;
    VkDevice vkDevice_ = VK_NULL_HANDLE;
    VkQueue vkQueue_ = VK_NULL_HANDLE;
    uint32_t queueFamilyIndex_ = 0;
    // 交换链
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_R8G8B8A8_SRGB;
    VkExtent2D swapchainExtent_ = {800, 600};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> framebuffers_;
    // MSAA 颜色缓冲
    std::vector<VkImage> msaaImages_;
    std::vector<VkDeviceMemory> msaaMemories_;
    std::vector<VkImageView> msaaViews_;
    // 渲染通道 + 共享管线布局
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE; // PushConstants (96 byte)
    // 命令
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;
    // 共享顶点/索引 (单位四边形)
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory_ = VK_NULL_HANDLE;
    // 同步
    // 改为数组（按 swapchain 图像索引）
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_; // fence 也改为 per-frame
    uint32_t frameIndex_ = 0;
    uint32_t currentImageIndex_ = 0;
    // 私有初始化
    bool createInstance(void *nativeHandle);
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    bool createSwapchain();
    void cleanupSwapchain();
    bool createRenderPass();
    bool createFramebuffers();
    bool createVertexBuffer();
    bool createCommandBuffers();
    bool createSyncObjects();
#if defined(__linux__)
    Display *x11Display_ = nullptr;
#endif
};