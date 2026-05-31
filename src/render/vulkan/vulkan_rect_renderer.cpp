module;
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <cstddef>
#include "rect_shaders.h"

module kwik.render.vulkan.rect_renderer;
import kwik.render.vulkan.context;
import kwik.core.types;
import std;
// ── PushConstants (96 byte, 必须与 rect.vert/rect.frag 对齐) ──────
namespace {
struct PushConstants {
    float topLeftX, topLeftY;                 // offset 0
    float sizeX, sizeY;                       // offset 8
    float fillR, fillG, fillB, fillA;         // offset 16
    float radius;                             // offset 32
    float borderWidth;                        // offset 36
    float _pad0, _pad1;                       // offset 40
    float borderR, borderG, borderB, borderA; // offset 48
    float opacity;                            // offset 64
    uint32_t drawMode;                        // offset 68 (0=fill, 1=stroke, 2=shadow)
    float shadowOffsetX, shadowOffsetY;       // offset 72
    float shadowBlur;                         // offset 80
    float _pad2;                              // offset 84
    float viewportW, viewportH;               // offset 88
};
static_assert(sizeof(PushConstants) == 96, "PushConstants must match shader layout");
} // namespace
RectRenderer::~RectRenderer() {
    destroy();
}
void RectRenderer::destroy() {
    if (fillPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, fillPipeline_, nullptr);
    if (strokePipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, strokePipeline_, nullptr);
    if (shadowPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, shadowPipeline_, nullptr);
    if (stencilPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, stencilPipeline_, nullptr); // ← 新增
    if (pipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    fillPipeline_ = strokePipeline_ = shadowPipeline_ = stencilPipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
}
// ================================================================
// create — 创建 fill / stroke / shadow 三条管线
// ================================================================
// ================================================================
// create — 创建 fill / stroke / shadow / stencil 四条管线
// ================================================================
bool RectRenderer::create(VulkanContext &ctx) {
    device_ = ctx.device();
    // ── 着色器 ───────────────────────────────────────────
    VkShaderModule vert =
        VulkanContext::createShaderModule(device_, kwik::shader::kRectVert, kwik::shader::kRectVertSize);
    VkShaderModule frag =
        VulkanContext::createShaderModule(device_, kwik::shader::kRectFrag, kwik::shader::kRectFragSize);
    if (!vert || !frag) return false;
    VkPipelineShaderStageCreateInfo stages[] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vert, "main"},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, frag, "main"},
    };
    VkVertexInputBindingDescription vtxBind{0, 2 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription vtxAttr{0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
    VkPipelineVertexInputStateCreateInfo vtxIn{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vtxIn.vertexBindingDescriptionCount = 1;
    vtxIn.pVertexBindingDescriptions = &vtxBind;
    vtxIn.vertexAttributeDescriptionCount = 1;
    vtxIn.pVertexAttributeDescriptions = &vtxAttr;
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
                                              VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkPipelineViewportStateCreateInfo vp{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                              nullptr,
                                              0,
                                              VK_FALSE,
                                              VK_FALSE,
                                              VK_POLYGON_MODE_FILL,
                                              VK_CULL_MODE_NONE,
                                              VK_FRONT_FACE_CLOCKWISE,
                                              VK_FALSE,
                                              0.0f,
                                              0.0f,
                                              0.0f,
                                              1.0f};
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;  // ─ canvas 1x, 不再取 ctx ─
    // ── 颜色混合 (fill / stroke / shadow 共用) ───────────
    VkPipelineColorBlendAttachmentState ba{};
    ba.blendEnable = VK_TRUE;
    ba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    ba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    ba.colorBlendOp = VK_BLEND_OP_ADD;
    ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    ba.alphaBlendOp = VK_BLEND_OP_ADD;
    ba.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1;
    blend.pAttachments = &ba;
    // ── Dynamic states (含 stencil 动态控制) ─────────────
    VkDynamicState dynStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,           VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_STENCIL_REFERENCE,  VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
        VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
    };
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 5;
    dyn.pDynamicStates = dynStates;
    // ── Depth / Stencil — 通用 (stencil 测试启用, 不写入) ─
    VkStencilOpState stencilNoWrite{};
    stencilNoWrite.failOp = VK_STENCIL_OP_KEEP;
    stencilNoWrite.passOp = VK_STENCIL_OP_KEEP;
    stencilNoWrite.depthFailOp = VK_STENCIL_OP_KEEP;
    stencilNoWrite.compareOp = VK_COMPARE_OP_EQUAL;
    stencilNoWrite.compareMask = 0xFF;
    stencilNoWrite.writeMask = 0x00; // 不写 stencil
    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;
    ds.stencilTestEnable = VK_TRUE;
    ds.front = stencilNoWrite;
    ds.back = stencilNoWrite;
    // ── Push constant range ──────────────────────────────
    VkPushConstantRange pcr{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants)};
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(device_, &pl, nullptr, &pipelineLayout_) != VK_SUCCESS) goto fail;
    // ── 创建 fill / stroke / shadow 管线 ─────────────────
    {
        VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pi.stageCount = 2;
        pi.pStages = stages;
        pi.pVertexInputState = &vtxIn;
        pi.pInputAssemblyState = &ia;
        pi.pViewportState = &vp;
        pi.pRasterizationState = &rs;
        pi.pMultisampleState = &ms;
        pi.pColorBlendState = &blend;
        pi.pDynamicState = &dyn;
        pi.layout = pipelineLayout_;
        pi.renderPass = ctx.renderPass();
        pi.subpass = 0;
        pi.pDepthStencilState = &ds; // ← 绑定 stencil 测试
        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &fillPipeline_) != VK_SUCCESS
            || vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &strokePipeline_) != VK_SUCCESS
            || vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &shadowPipeline_) != VK_SUCCESS)
            goto fail;
    }
    // ── 创建 stencil mask 管线 (颜色输出禁用, stencil 写入) ──
    {
        // 颜色: 全部通道禁写 → 不影响帧缓冲颜色
        VkPipelineColorBlendAttachmentState sba{};
        // blendEnable = VK_FALSE (default), colorWriteMask = 0 (default) → 不写
        VkPipelineColorBlendStateCreateInfo sBlend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        sBlend.attachmentCount = 1;
        sBlend.pAttachments = &sba;
        // Stencil: ALWAYS 通过比较 → 总是写入 ref 值
        VkStencilOpState stencilWrite{};
        stencilWrite.failOp = VK_STENCIL_OP_KEEP;
        stencilWrite.passOp = VK_STENCIL_OP_REPLACE;
        stencilWrite.depthFailOp = VK_STENCIL_OP_KEEP;
        stencilWrite.compareOp = VK_COMPARE_OP_ALWAYS;
        stencilWrite.compareMask = 0xFF;
        stencilWrite.writeMask = 0xFF;
        VkPipelineDepthStencilStateCreateInfo sDs{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        sDs.stencilTestEnable = VK_TRUE;
        sDs.front = stencilWrite;
        sDs.back = stencilWrite;
        VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pi.stageCount = 2;
        pi.pStages = stages;
        pi.pVertexInputState = &vtxIn;
        pi.pInputAssemblyState = &ia;
        pi.pViewportState = &vp;
        pi.pRasterizationState = &rs;
        pi.pMultisampleState = &ms;
        pi.pColorBlendState = &sBlend; // 颜色不写
        pi.pDynamicState = &dyn;
        pi.layout = pipelineLayout_;
        pi.renderPass = ctx.renderPass();
        pi.subpass = 0;
        pi.pDepthStencilState = &sDs; // stencil 写入
        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &stencilPipeline_) != VK_SUCCESS)
            goto fail;
    }
    vkDestroyShaderModule(device_, vert, nullptr);
    vkDestroyShaderModule(device_, frag, nullptr);
    return true;
fail:
    vkDestroyShaderModule(device_, vert, nullptr);
    vkDestroyShaderModule(device_, frag, nullptr);
    destroy();
    return false;
}
// ================================================================
// 绘制方法
// ================================================================
void RectRenderer::clear(VulkanContext &ctx, const Color &color) {
    VkClearAttachment att{};
    att.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    att.clearValue.color = {{color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f}};
    VkClearRect cr{{{0, 0}, ctx.extent()}, 0, 1};
    vkCmdClearAttachments(ctx.commandBuffer(), 1, &att, 1, &cr);
}
void RectRenderer::fillRect(VulkanContext &ctx, const Rect &rect, const Color &color) {
    fillRoundedRect(ctx, rect, 0, color, 1.0f);
}
void RectRenderer::fillRoundedRect(VulkanContext &ctx, const Rect &rect, float radius, const Color &color,
                                   float globalAlpha) {
    VkCommandBuffer cb = ctx.commandBuffer();
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, fillPipeline_);
    PushConstants pc{};
    pc.topLeftX = rect.x;
    pc.topLeftY = rect.y;
    pc.sizeX = rect.width;
    pc.sizeY = rect.height;
    pc.fillR = color.r / 255.f;
    pc.fillG = color.g / 255.f;
    pc.fillB = color.b / 255.f;
    pc.fillA = color.a / 255.f;
    pc.radius = radius;
    pc.opacity = globalAlpha;
    pc.drawMode = 0;
    pc.viewportW = (float)ctx.extent().width;
    pc.viewportH = (float)ctx.extent().height;
    vkCmdPushConstants(cb, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(PushConstants), &pc);
    VkBuffer vb = ctx.vertexBuffer();
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &vb, &off);
    vkCmdBindIndexBuffer(cb, ctx.indexBuffer(), 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cb, 6, 1, 0, 0, 0);
}
void RectRenderer::strokeRoundedRect(VulkanContext &ctx, const Rect &rect, float radius, const Color &color,
                                     float strokeWidth, float globalAlpha) {
    VkCommandBuffer cb = ctx.commandBuffer();
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, strokePipeline_);
    PushConstants pc{};
    pc.topLeftX = rect.x;
    pc.topLeftY = rect.y;
    pc.sizeX = rect.width;
    pc.sizeY = rect.height;
    pc.radius = radius;
    pc.borderWidth = strokeWidth;
    pc.borderR = color.r / 255.f;
    pc.borderG = color.g / 255.f;
    pc.borderB = color.b / 255.f;
    pc.borderA = color.a / 255.f;
    pc.opacity = globalAlpha;
    pc.drawMode = 1;
    pc.viewportW = (float)ctx.extent().width;
    pc.viewportH = (float)ctx.extent().height;
    vkCmdPushConstants(cb, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(PushConstants), &pc);
    VkBuffer vb = ctx.vertexBuffer();
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &vb, &off);
    vkCmdBindIndexBuffer(cb, ctx.indexBuffer(), 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cb, 6, 1, 0, 0, 0);
}

// 用 4 层同心 SDF 矩形叠加逼近高斯模糊衰减：半径以 blurRadius/3 步进递增，透明度逐层衰减
// kLayerAlphas 权重和为 0.45 × shadow.color.a/255，叠加后总体约原始阴影透明度的 45%。如果觉得太淡可以调高数组值。
// 无模糊时 (blur <= 0.5) 退化为单层，kLayerAlphas[0] = 0.20，用户可通过调高 rgba() 的 alpha 值来补偿
void RectRenderer::drawShadow(VulkanContext &ctx, const Rect &rect, float radius, const Shadow &shadow,
                              float globalAlpha) {
    VkCommandBuffer cb = ctx.commandBuffer();
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline_);
    VkBuffer vb = ctx.vertexBuffer();
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &vb, &off);
    vkCmdBindIndexBuffer(cb, ctx.indexBuffer(), 0, VK_INDEX_TYPE_UINT16);
    // ── 多层阴影: 模拟高斯模糊衰减 ──────────────────────────
    // 每层半径按 blurRadius/3 递增, 透明度 layerAlpha[N] 递减,
    // 叠加后近似连续模糊效果。
    // 层数: 4 层 (无 blur 时退化为 1 层, 避免空绘制)
    static constexpr int kShadowLayers = 4;
    static const float kLayerAlphas[kShadowLayers] = {0.20f, 0.14f, 0.08f, 0.03f};
    const float blur = shadow.blurRadius;
    // 若无模糊半径, 回退为单层 (保证 100% 阴影颜色可见)
    const int layers = (blur <= 0.5f) ? 1 : kShadowLayers;
    const float step = (layers > 1) ? (blur / float(layers - 1)) : 0.0f;
    for (int i = 0; i < layers; i++) {
        PushConstants pc{};
        pc.topLeftX = rect.x + shadow.offsetX;
        pc.topLeftY = rect.y + shadow.offsetY;
        pc.sizeX = rect.width;
        pc.sizeY = rect.height;
        pc.fillR = shadow.color.r / 255.f;
        pc.fillG = shadow.color.g / 255.f;
        pc.fillB = shadow.color.b / 255.f;
        pc.fillA = shadow.color.a / 255.f;
        pc.radius = radius;
        // 阴影半径: 基础圆角半径 + 该层模糊增量
        pc.shadowBlur = step * float(i);
        // 透明度: 基础 globalAlpha * 颜色 alpha * 该层权重
        pc.opacity = globalAlpha * (shadow.color.a / 255.f) * kLayerAlphas[i];
        pc.drawMode = 2; // shadow mode
        pc.viewportW = (float)ctx.extent().width;
        pc.viewportH = (float)ctx.extent().height;
        vkCmdPushConstants(cb, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(PushConstants), &pc);
        vkCmdDrawIndexed(cb, 6, 1, 0, 0, 0);
    }
}

// ================================================================
// writeStencilMask — 将圆角矩形 SDF 写入 stencil buffer
// ================================================================
void RectRenderer::writeStencilMask(VulkanContext &ctx, const Rect &rect, float radius) {
    VkCommandBuffer cb = ctx.commandBuffer();
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, stencilPipeline_);
    // 绑定几何数据
    VkBuffer vb = ctx.vertexBuffer();
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &vb, &off);
    vkCmdBindIndexBuffer(cb, ctx.indexBuffer(), 0, VK_INDEX_TYPE_UINT16);
    // push constants (填充模式, 任意颜色 — 颜色写入已被管线禁用)
    PushConstants pc{};
    pc.topLeftX = rect.x;
    pc.topLeftY = rect.y;
    pc.sizeX = rect.width;
    pc.sizeY = rect.height;
    pc.fillR = 1.0f; // 占位 (颜色不写入)
    pc.fillG = 1.0f;
    pc.fillB = 1.0f;
    pc.fillA = 1.0f;
    pc.radius = radius;
    pc.opacity = 1.0f;
    pc.drawMode = 0; // fill mode
    pc.viewportW = (float)ctx.extent().width;
    pc.viewportH = (float)ctx.extent().height;
    vkCmdPushConstants(cb, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(PushConstants), &pc);
    vkCmdDrawIndexed(cb, 6, 1, 0, 0, 0); // 写入 stencil=ref 到圆角区域
}
// ================================================================
// disableStencilTest — 关闭 stencil 测试 (恢复无裁剪状态)
// ================================================================
void RectRenderer::disableStencilTest(VulkanContext &ctx) {
    // 将 compareMask 设为 0x00 → EQUAL 比较恒成立 → 等效禁用 stencil test
    vkCmdSetStencilCompareMask(ctx.commandBuffer(), VK_STENCIL_FACE_FRONT_AND_BACK, 0x00);
}