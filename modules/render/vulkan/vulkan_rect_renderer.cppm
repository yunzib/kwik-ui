module;
#include <vulkan/vulkan.h>
export module kwik.render.vulkan.rect_renderer;
import kwik.core.types;
import kwik.render.vulkan.context;
/**
 * @brief 形状渲染器 — fill / stroke / shadow
 *
 * 三条管线共享同一 PushConstants(96 byte) + pipelineLayout_。
 * 每帧从 VulkanContext 获取当前 commandBuffer / vertexBuffer 等。
 */
export class RectRenderer {
public:
    RectRenderer() = default;
    ~RectRenderer();
    bool create(VulkanContext &ctx);
    void destroy();
    void clear(VulkanContext &ctx, const Color &color);
    void fillRect(VulkanContext &ctx, const Rect &rect, const Color &color);
    void fillRoundedRect(VulkanContext &ctx, const Rect &rect, float radius, const Color &color, float globalAlpha);
    void strokeRoundedRect(VulkanContext &ctx, const Rect &rect, float radius, const Color &color, float strokeWidth,
                           float globalAlpha);
    void drawShadow(VulkanContext &ctx, const Rect &rect, float radius, const Shadow &shadow, float globalAlpha);
    VkPipelineLayout layout() const {
        return pipelineLayout_;
    }

    // ── Stencil 裁剪支持 ────────────────────────────────
    /// 用 fill shader 绘制 SDF 圆角矩形到 stencil 缓冲区 (颜色输出禁用)
    void writeStencilMask(VulkanContext &ctx, const Rect &rect, float radius);
    /// 关闭 stencil 测试 (恢复无裁剪状态)
    void disableStencilTest(VulkanContext &ctx);

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline fillPipeline_ = VK_NULL_HANDLE;
    VkPipeline strokePipeline_ = VK_NULL_HANDLE;
    VkPipeline shadowPipeline_ = VK_NULL_HANDLE;
    VkPipeline stencilPipeline_ = VK_NULL_HANDLE;
};