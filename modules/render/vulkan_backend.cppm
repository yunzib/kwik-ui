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

export module kwik.render.vulkan_backend;

import kwik.render.backend;
import kwik.core.types;
import kwik.render.command;
import std;

/**
 * @brief Vulkan渲染后端实现
 */
export class VulkanBackend : public RenderBackend {
public:
    VulkanBackend();
    VulkanBackend(int width, int height);
    ~VulkanBackend() override;

    bool initialize(void *nativeHandle, int width, int height) override;
    void shutdown() override;
    void resize(int width, int height) override;
    bool beginFrame() override;
    void endFrame() override;
    void present() override;
    void setGlobalAlpha(float alpha) override;
    void pushClipRoundedRect(const Rect &rect, float radius) override;
    void resetClip() override;
    void clear(const Color &color) override;
    void fillRect(const Rect &rect, const Color &color) override;
    void fillRoundedRect(const Rect &rect, float radius, const Color &color) override;
    void strokeRoundedRect(const Rect &rect, float radius, const Color &color, float strokeWidth) override;
    void drawShadow(const Rect &rect, float radius, const Shadow &shadow) override;
    void drawGlyph(const DrawGlyphCmd &cmd) override;
    void uploadGlyphAtlas(const uint8_t *data, uint32_t width, uint32_t height) override;
    void drawImage(const DrawImageCmd &cmd) override;
    uint32_t createImageTexture(const uint8_t *rgba, uint32_t width, uint32_t height) override;
    void destroyImageTexture(uint32_t id) override;

    void saveState() override;
    void restoreState() override;

    BackendType getType() const override {
        return BackendType::Vulkan;
    }
    int getWidth() const override {
        return width_;
    }
    int getHeight() const override {
        return height_;
    }

private:
    int width_ = 0;
    int height_ = 0;
    float globalAlpha_ = 1.0f;
    Color clearColor_{245, 245, 245, 255};
    // ── Vulkan 核心 ───────────────────────────────────────────
    VkInstance vkInstance_ = VK_NULL_HANDLE;
    VkSurfaceKHR vkSurface_ = VK_NULL_HANDLE;
    VkPhysicalDevice vkPhysicalDevice_ = VK_NULL_HANDLE;
    VkDevice vkDevice_ = VK_NULL_HANDLE;
    VkQueue vkQueue_ = VK_NULL_HANDLE;
    uint32_t queueFamilyIndex_ = 0;
    // ── Swapchain ─────────────────────────────────────────────
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D swapchainExtent_ = {800, 600};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> framebuffers_;
    // ── Render pass / Pipeline ────────────────────────────────
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline fillPipeline_ = VK_NULL_HANDLE;
    VkPipeline strokePipeline_ = VK_NULL_HANDLE;
    VkPipeline shadowPipeline_ = VK_NULL_HANDLE;
    VkPipeline glyphPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout glyphPipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout glyphDescSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool glyphDescPool_ = VK_NULL_HANDLE;
    VkDescriptorSet glyphDescSet_ = VK_NULL_HANDLE;
    VkImage glyphAtlasImage_ = VK_NULL_HANDLE;
    VkDeviceMemory glyphAtlasMemory_ = VK_NULL_HANDLE;
    VkImageView glyphAtlasView_ = VK_NULL_HANDLE;
    VkSampler glyphAtlasSampler_ = VK_NULL_HANDLE;
    // ── Command ───────────────────────────────────────────────
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;
    // ── Vertex / Index ────────────────────────────────────────
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory_ = VK_NULL_HANDLE;
    // ── Sync ──────────────────────────────────────────────────
    VkSemaphore imageAvailableSemaphore_ = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore_ = VK_NULL_HANDLE;
    VkFence inFlightFence_ = VK_NULL_HANDLE;
    uint32_t currentImageIndex_ = 0;

    // ── Image pipeline / textures ──────────────────────────────
    struct ImageTextureData {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkDescriptorSet descSet = VK_NULL_HANDLE;
        uint32_t width = 0;
        uint32_t height = 0;
    };
    std::unordered_map<uint32_t, ImageTextureData> imageTextures_;
    uint32_t nextTextureId_ = 1;
    VkPipeline imagePipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout imagePipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout imageDescSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool imageDescPool_ = VK_NULL_HANDLE;

    // ── 私有辅助方法 ──────────────────────────────────────────
    bool initVulkan(void *nativeHandle, int width, int height);
    void cleanupVulkan();
    bool createSwapchain();
    void cleanupSwapchain();
    bool createRenderPass();
    bool createFramebuffers();
    bool createPipeline();
    VkShaderModule createShaderModule(const std::uint8_t *spv, std::size_t size);
    bool createVertexBuffer();
    bool createCommandBuffers();
    bool createSyncObjects();
    bool createGlyphPipeline();
    bool createGlyphAtlas();
    void cleanupGlyphResources();

    bool createImagePipeline();
    void cleanupImageResources();

    // ── 裁剪状态管理 ─────────────────────────────────────────
    std::vector<Rect> clipStack_;                  // 裁剪矩形栈
    std::vector<std::vector<Rect>> clipSaveStack_; // save/restore 裁剪状态栈
    std::vector<float> alphaSaveStack_;

    // 工具函数
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props);
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer &buffer,
                      VkDeviceMemory &memory);
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);

#if defined(__linux__)
    Display *x11Display_ = nullptr;
#endif
};
