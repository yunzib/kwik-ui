module;
#include <vulkan/vulkan.h>
#include <vector>
export module kwik.render.vulkan.glyph_renderer;
import kwik.core.types;
import kwik.render.vulkan.context;
import kwik.render.command;
/**
 * @brief 文字渲染器 — SDF 字形管线和图集
 *
 * 2048x2048 R8_UNORM 图集，GlyphPushConstants(56 byte) 布局。
 * 图集上传通过一次性命令提交，独立于主渲染通道。
 */
export class GlyphRenderer {
public:
    GlyphRenderer() = default;
    ~GlyphRenderer();
    bool create(VulkanContext &ctx);
    void destroy();
    void uploadAtlas(VulkanContext &ctx, const uint8_t *data,
                     uint32_t width, uint32_t height);
    void drawGlyph(VulkanContext &ctx, const DrawGlyphCmd &cmd,
                   float globalAlpha);
private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPipeline            glyphPipeline_      = VK_NULL_HANDLE;
    VkPipelineLayout      glyphPipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout glyphDescSetLayout_  = VK_NULL_HANDLE;
    VkDescriptorPool      glyphDescPool_       = VK_NULL_HANDLE;
    VkDescriptorSet       glyphDescSet_        = VK_NULL_HANDLE;
    VkImage        glyphAtlasImage_  = VK_NULL_HANDLE;
    VkDeviceMemory glyphAtlasMemory_ = VK_NULL_HANDLE;
    VkImageView    glyphAtlasView_   = VK_NULL_HANDLE;
    VkSampler      glyphAtlasSampler_ = VK_NULL_HANDLE;
};