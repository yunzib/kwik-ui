module;
#include <vulkan/vulkan.h>
#include <vector>
export module kwik.render.vulkan.glyph_renderer;
import kwik.core.types;
import kwik.render.command;
import kwik.render.vulkan.context;
import kwik.render.text.types;


export class GlyphRenderer {
public:
    GlyphRenderer() = default;
    ~GlyphRenderer();
    bool create(VkDevice device, VkPhysicalDevice physDevice,
            VkCommandPool cmdPool, VkQueue queue,
            VkRenderPass renderPass,
            VkBuffer vertexBuffer, VkBuffer indexBuffer);
    void destroy();

    void uploadPendingGlyphs(const DeviceContext &dc);
    void drawGlyph(VkCommandBuffer cb, VkExtent2D extent, const DrawGlyphCmd &cmd, float globalAlpha,
                   const Transform2D &t);
    void drawGlyphClipped(VkCommandBuffer cb, VkExtent2D extent, const DrawGlyphCmd &cmd, float globalAlpha,
                          const Transform2D &t);

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkPipeline glyphPipeline_ = VK_NULL_HANDLE;
    VkPipeline glyphClipPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout glyphPipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout glyphDescSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool glyphDescPool_ = VK_NULL_HANDLE;
    VkDescriptorSet glyphDescSet_ = VK_NULL_HANDLE;
    VkImage glyphAtlasImage_ = VK_NULL_HANDLE;
    VkDeviceMemory glyphAtlasMemory_ = VK_NULL_HANDLE;
    VkImageView glyphAtlasView_ = VK_NULL_HANDLE;
    VkSampler glyphAtlasSampler_ = VK_NULL_HANDLE;
    VkImageLayout atlasLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;  // ← NEW: 跟踪图集 layout
};