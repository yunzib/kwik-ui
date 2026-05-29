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
    if (pipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    fillPipeline_ = strokePipeline_ = shadowPipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
}
// ================================================================
// create — 创建 fill / stroke / shadow 三条管线
// ================================================================
bool RectRenderer::create(VulkanContext &ctx) {
    device_ = ctx.device();
    // 着色器
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
    ms.rasterizationSamples = ctx.msaaSamples();
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
    VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynStates;
    VkPushConstantRange pcr{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants)};
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(device_, &pl, nullptr, &pipelineLayout_) != VK_SUCCESS) goto fail;
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
        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &fillPipeline_) != VK_SUCCESS
            || vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &strokePipeline_) != VK_SUCCESS
            || vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &shadowPipeline_) != VK_SUCCESS)
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
void RectRenderer::drawShadow(VulkanContext &ctx, const Rect &rect, float radius, const Shadow &shadow,
                              float globalAlpha) {
    VkCommandBuffer cb = ctx.commandBuffer();
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline_);
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
    pc.opacity = globalAlpha;
    pc.drawMode = 2;
    pc.shadowBlur = shadow.blurRadius;
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