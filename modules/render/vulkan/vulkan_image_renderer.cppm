module;
#include <vulkan/vulkan.h>
#include <unordered_map>
export module kwik.render.vulkan.image_renderer;
import kwik.core.types;
import kwik.render.command;
import kwik.render.vulkan.context;

export class ImageRenderer {
public:
    ImageRenderer() = default;
    ~ImageRenderer();
    bool create(VkDevice device, VkPhysicalDevice physDevice, VkRenderPass renderPass,
                VkBuffer vertexBuffer, VkBuffer indexBuffer);
    void destroy();

    uint32_t createTexture(const DeviceContext &dc, const uint8_t *rgba, uint32_t w, uint32_t h);
    void destroyTexture(uint32_t id);
    void drawImage(VkCommandBuffer cb, VkExtent2D extent, const DrawImageCmd &cmd, float globalAlpha);
    void drawImageClipped(VkCommandBuffer cb, VkExtent2D extent, const DrawImageCmd &cmd, float globalAlpha);

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkPipeline imagePipeline_ = VK_NULL_HANDLE;
    VkPipeline imageClipPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout imagePipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout imageDescSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool imageDescPool_ = VK_NULL_HANDLE;
    struct TextureData {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkDescriptorSet descSet = VK_NULL_HANDLE;
        uint32_t width = 0;
        uint32_t height = 0;
    };
    std::unordered_map<uint32_t, TextureData> textures_;
    uint32_t nextId_ = 1;
};