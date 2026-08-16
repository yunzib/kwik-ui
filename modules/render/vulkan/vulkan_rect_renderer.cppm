module;
#include <vulkan/vulkan.h>
export module kwik.render.vulkan.rect_renderer;
import kwik.core.types;

export class RectRenderer {
public:
    RectRenderer() = default;
    ~RectRenderer();
    bool create(VkDevice device, VkRenderPass renderPass, VkBuffer vertexBuffer, VkBuffer indexBuffer);
    void destroy();

    void clear(VkCommandBuffer cmd, VkExtent2D extent, const Color &color);
    void fillRect(VkCommandBuffer cmd, VkExtent2D extent, const Rect &rect, const Color &color, const Transform2D &t);
    void fillRoundedRect(VkCommandBuffer cmd, VkExtent2D extent, const Rect &rect, float radius, const Color &color,
                         float globalAlpha, const Transform2D &t);
    void fillRoundedRect(VkCommandBuffer cmd, VkExtent2D extent, const Rect &rect, float radius, const Color &color,
                         const Gradient &gradient, float globalAlpha, const Transform2D &t);
    /** @brief 线段胶囊描边 */
    void drawSegment(VkCommandBuffer cmd, VkExtent2D extent, float ax, float ay, float bx, float by, float halfW,
                     const Color &color, float globalAlpha, const Transform2D &t);
    void strokeRoundedRect(VkCommandBuffer cmd, VkExtent2D extent, const Rect &rect, float radius, const Color &color,
                           float strokeWidth, float globalAlpha, const Transform2D &t);
    void drawShadow(VkCommandBuffer cmd, VkExtent2D extent, const Rect &rect, float radius, const Shadow &shadow,
                    float globalAlpha, const Transform2D &t);
    void writeStencilMask(VkCommandBuffer cmd, VkExtent2D extent, const Rect &rect, float radius, const Transform2D &t);
    void disableStencilTest(VkCommandBuffer cmd);

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline fillPipeline_ = VK_NULL_HANDLE;
    VkPipeline strokePipeline_ = VK_NULL_HANDLE;
    VkPipeline shadowPipeline_ = VK_NULL_HANDLE;
    VkPipeline stencilPipeline_ = VK_NULL_HANDLE;
};