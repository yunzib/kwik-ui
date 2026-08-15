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
    float topLeftX, topLeftY;
    float sizeX, sizeY;
    float fillR, fillG, fillB, fillA;
    float radius;
    float borderWidth;
    float _pad0, _pad1;
    float borderR, borderG, borderB, borderA;
    float opacity;
    uint32_t drawMode;
    float shadowOffsetX, shadowOffsetY;
    float shadowBlur;
    float _pad2;
    float viewportW, viewportH;
    float m00, m01, m02, m10, m11, m12;    // ← 矩阵
};
static_assert(sizeof(PushConstants) == 120, "PushConstants size mismatch");
}    // namespace

RectRenderer::~RectRenderer() {
    destroy();
}
void RectRenderer::destroy() {
    if (fillPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, fillPipeline_, nullptr);
    if (strokePipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, strokePipeline_, nullptr);
    if (shadowPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, shadowPipeline_, nullptr);
    if (stencilPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, stencilPipeline_, nullptr);
    if (pipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    fillPipeline_ = strokePipeline_ = shadowPipeline_ = stencilPipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
}
// ================================================================
// create — 创建 fill / stroke / shadow / stencil 四条管线
// ================================================================
bool RectRenderer::create(VkDevice device, VkRenderPass renderPass, VkBuffer vertexBuffer, VkBuffer indexBuffer) {
    device_ = device;
    vertexBuffer_ = vertexBuffer;
    indexBuffer_ = indexBuffer;
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
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    // ── 颜色混合 ──────────────────────────────────────────
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
    // ── Dynamic states ────────────────────────────────────
    VkDynamicState dynStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,           VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_STENCIL_REFERENCE,  VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
        VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
    };
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 5;
    dyn.pDynamicStates = dynStates;
    // ── Depth / Stencil ────────────────────────────────────
    VkStencilOpState stencilNoWrite{};
    stencilNoWrite.failOp = VK_STENCIL_OP_KEEP;
    stencilNoWrite.passOp = VK_STENCIL_OP_KEEP;
    stencilNoWrite.depthFailOp = VK_STENCIL_OP_KEEP;
    stencilNoWrite.compareOp = VK_COMPARE_OP_EQUAL;
    stencilNoWrite.compareMask = 0xFF;
    stencilNoWrite.writeMask = 0x00;
    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;
    ds.stencilTestEnable = VK_TRUE;
    ds.front = stencilNoWrite;
    ds.back = stencilNoWrite;
    // ── Push constant range ────────────────────────────────
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
        pi.renderPass = renderPass;
        pi.subpass = 0;
        pi.pDepthStencilState = &ds;
        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &fillPipeline_) != VK_SUCCESS
            || vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &strokePipeline_) != VK_SUCCESS
            || vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &shadowPipeline_) != VK_SUCCESS)
            goto fail;
    }
    // ── 创建 stencil mask 管线 ────────────────────────────
    {
        VkPipelineColorBlendAttachmentState sba{};
        VkPipelineColorBlendStateCreateInfo sBlend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        sBlend.attachmentCount = 1;
        sBlend.pAttachments = &sba;
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
        pi.pColorBlendState = &sBlend;
        pi.pDynamicState = &dyn;
        pi.layout = pipelineLayout_;
        pi.renderPass = renderPass;
        pi.subpass = 0;
        pi.pDepthStencilState = &sDs;
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
void RectRenderer::clear(VkCommandBuffer cmd, VkExtent2D extent, const Color &color) {
    VkClearAttachment att{};
    att.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    att.clearValue.color = {{color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f}};
    VkClearRect cr{{{0, 0}, extent}, 0, 1};
    vkCmdClearAttachments(cmd, 1, &att, 1, &cr);
}


void RectRenderer::fillRect(VkCommandBuffer cmd, VkExtent2D extent, const Rect &rect, const Color &color,
                            const Transform2D &t) {
    fillRoundedRect(cmd, extent, rect, 0, color, 1.0f, t);
}

void RectRenderer::fillRoundedRect(VkCommandBuffer cmd, VkExtent2D extent, const Rect &rect, float radius,
                                   const Color &color, float globalAlpha, const Transform2D &t) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fillPipeline_);
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
    pc.viewportW = static_cast<float>(extent.width);
    pc.viewportH = static_cast<float>(extent.height);
    pc.m00 = t.m00;
    pc.m01 = t.m01;
    pc.m02 = t.m02;
    pc.m10 = t.m10;
    pc.m11 = t.m11;
    pc.m12 = t.m12;
    vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(PushConstants), &pc);
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer_, &off);
    vkCmdBindIndexBuffer(cmd, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
}

void RectRenderer::drawSegment(VkCommandBuffer cmd, VkExtent2D extent, float ax, float ay, float bx, float by,
                               float halfW, const Color &color, float globalAlpha, const Transform2D &t) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fillPipeline_);
    // 胶囊包围盒 = 两端点 ± halfW
    float x0 = std::min(ax, bx) - halfW;
    float y0 = std::min(ay, by) - halfW;
    float w = std::abs(bx - ax) + 2.0f * halfW;
    float h = std::abs(by - ay) + 2.0f * halfW;

    PushConstants pc{};
    pc.topLeftX = x0;
    pc.topLeftY = y0;
    pc.sizeX = w;
    pc.sizeY = h;
    pc.fillR = color.r / 255.f;
    pc.fillG = color.g / 255.f;
    pc.fillB = color.b / 255.f;
    pc.fillA = color.a / 255.f;
    pc.radius = halfW;    // 胶囊半径
    pc.opacity = globalAlpha;
    pc.drawMode = 3;    // segment 模式
    // 复用 borderColor 字段把线段端点（包围盒坐标）传给 shader
    pc.borderR = ax - x0;
    pc.borderG = ay - y0;    // a.xy
    pc.borderB = bx - x0;
    pc.borderA = by - y0;    // b.xy
    pc.viewportW = static_cast<float>(extent.width);
    pc.viewportH = static_cast<float>(extent.height);
    pc.m00 = t.m00;
    pc.m01 = t.m01;
    pc.m02 = t.m02;
    pc.m10 = t.m10;
    pc.m11 = t.m11;
    pc.m12 = t.m12;
    vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(PushConstants), &pc);
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer_, &off);
    vkCmdBindIndexBuffer(cmd, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
}

void RectRenderer::strokeRoundedRect(VkCommandBuffer cmd, VkExtent2D extent, const Rect &rect, float radius,
                                     const Color &color, float strokeWidth, float globalAlpha, const Transform2D &t) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, strokePipeline_);
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
    pc.viewportW = static_cast<float>(extent.width);
    pc.viewportH = static_cast<float>(extent.height);
    pc.m00 = t.m00;
    pc.m01 = t.m01;
    pc.m02 = t.m02;
    pc.m10 = t.m10;
    pc.m11 = t.m11;
    pc.m12 = t.m12;
    vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(PushConstants), &pc);
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer_, &off);
    vkCmdBindIndexBuffer(cmd, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
}
void RectRenderer::drawShadow(VkCommandBuffer cmd, VkExtent2D extent, const Rect &rect, float radius,
                              const Shadow &shadow, float globalAlpha, const Transform2D &t) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline_);
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer_, &off);
    vkCmdBindIndexBuffer(cmd, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    static constexpr int kShadowLayers = 4;
    static const float kLayerAlphas[kShadowLayers] = {0.20f, 0.14f, 0.08f, 0.03f};
    const float blur = shadow.blurRadius;
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
        pc.shadowBlur = step * float(i);
        pc.opacity = globalAlpha * (shadow.color.a / 255.f) * kLayerAlphas[i];
        pc.drawMode = 2;
        pc.viewportW = static_cast<float>(extent.width);
        pc.viewportH = static_cast<float>(extent.height);
        pc.m00 = t.m00;
        pc.m01 = t.m01;
        pc.m02 = t.m02;
        pc.m10 = t.m10;
        pc.m11 = t.m11;
        pc.m12 = t.m12;
        vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(PushConstants), &pc);
        vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
    }
}
// ================================================================
// writeStencilMask — 将圆角矩形 SDF 写入 stencil buffer
// ================================================================
void RectRenderer::writeStencilMask(VkCommandBuffer cmd, VkExtent2D extent, const Rect &rect, float radius,
                                    const Transform2D &t) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, stencilPipeline_);
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer_, &off);
    vkCmdBindIndexBuffer(cmd, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    PushConstants pc{};
    pc.topLeftX = rect.x;
    pc.topLeftY = rect.y;
    pc.sizeX = rect.width;
    pc.sizeY = rect.height;
    pc.fillR = 1.0f;
    pc.fillG = 1.0f;
    pc.fillB = 1.0f;
    pc.fillA = 1.0f;
    pc.radius = radius;
    pc.opacity = 1.0f;
    pc.drawMode = 0;
    pc.viewportW = static_cast<float>(extent.width);
    pc.viewportH = static_cast<float>(extent.height);
    pc.m00 = t.m00;
    pc.m01 = t.m01;
    pc.m02 = t.m02;
    pc.m10 = t.m10;
    pc.m11 = t.m11;
    pc.m12 = t.m12;
    vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(PushConstants), &pc);
    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
}
// ================================================================
// disableStencilTest — 关闭 stencil 测试
// ================================================================
void RectRenderer::disableStencilTest(VkCommandBuffer cmd) {
    vkCmdSetStencilCompareMask(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 0x00);
}