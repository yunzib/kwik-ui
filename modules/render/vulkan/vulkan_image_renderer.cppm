module;
#include <vulkan/vulkan.h>
#include <unordered_map>
export module kwik.render.vulkan.image_renderer;
import kwik.core.types;
import kwik.render.vulkan.context;
import kwik.render.command;
/**
 * @brief 图片渲染器 — 独立纹理管线 (mipmap 自动生成)
 *
 * 复用 GlyphPushConstants(56 byte) 布局。
 * 每张纹理拥有独立的 VkImage / VkImageView / VkSampler / VkDescriptorSet。
 */
export class ImageRenderer {
public:
    ImageRenderer() = default;
    ~ImageRenderer();
    bool create(VulkanContext &ctx);
    void destroy();
    uint32_t createTexture(VulkanContext &ctx, const uint8_t *rgba,
                           uint32_t width, uint32_t height);
    void     destroyTexture(uint32_t id);
    void     drawImage(VulkanContext &ctx, const DrawImageCmd &cmd,
                       float globalAlpha);
private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPipeline            imagePipeline_       = VK_NULL_HANDLE;
    VkPipelineLayout      imagePipelineLayout_  = VK_NULL_HANDLE;
    VkDescriptorSetLayout imageDescSetLayout_   = VK_NULL_HANDLE;
    VkDescriptorPool      imageDescPool_        = VK_NULL_HANDLE;
    struct TextureData {
        VkImage         image   = VK_NULL_HANDLE;
        VkDeviceMemory  memory  = VK_NULL_HANDLE;
        VkImageView     view    = VK_NULL_HANDLE;
        VkSampler       sampler = VK_NULL_HANDLE;
        VkDescriptorSet descSet = VK_NULL_HANDLE;
        uint32_t width  = 0;
        uint32_t height = 0;
    };
    std::unordered_map<uint32_t, TextureData> textures_;
    uint32_t nextId_ = 1;
};