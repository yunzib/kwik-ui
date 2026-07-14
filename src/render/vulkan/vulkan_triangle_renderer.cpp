/**
 * @file vulkan_triangle_renderer.cpp
 * @brief 三角形网格渲染器实现
 */

module;
#include <vulkan/vulkan.h>
#include <cstddef>
#include <cstring>
#include "triangle_shaders.h"

module kwik.render.vulkan.triangle_renderer;

import kwik.render.vulkan.context;
import kwik.core.types;
import kwik.core.path;

import std;

namespace {
/**
 * @brief Push constants 布局（必须与 shader 对齐）
 * 总计 36 bytes: color(16) + viewport(8) + opacity(4) + pad(8)
 */
struct PushConstants {
    float r, g, b, a;      // color      (offset 0)
    float viewportW;       // viewport   (offset 16)
    float viewportH;       //            (offset 20)
    float opacity;         //            (offset 24)
    float _pad0, _pad1;    // pad        (offset 28)
};
static_assert(sizeof(PushConstants) == 36, "PushConstants size mismatch with shader");
}    // anonymous namespace

TriangleRenderer::~TriangleRenderer() {
    destroy();
}

void TriangleRenderer::destroy() {
    if (pipeline_ != VK_NULL_HANDLE) { vkDestroyPipeline(device_, pipeline_, nullptr); }
    if (pipelineLayout_ != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr); }
    pipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;

    VkBuffer oldVB = stagingBuffer_;
    VkDeviceMemory oldMem = stagingMemory_;
    void *oldMap = mappedData_;
    stagingBuffer_ = VK_NULL_HANDLE;
    stagingMemory_ = VK_NULL_HANDLE;
    mappedData_ = nullptr;
    if (oldMap) vkUnmapMemory(device_, oldMem);
    if (oldMem) vkFreeMemory(device_, oldMem, nullptr);
    if (oldVB) vkDestroyBuffer(device_, oldVB, nullptr);
}

bool TriangleRenderer::create(VkDevice device, VkPhysicalDevice physDevice, VkRenderPass renderPass, VkBuffer vertexBuffer,
                VkBuffer indexBuffer) {
    device_ = device;
    physDevice_ = physDevice;
    vertexBuffer_ = vertexBuffer;
    indexBuffer_ = indexBuffer;

    // ── 加载 shader ──
    VkShaderModule vert =
        VulkanContext::createShaderModule(device_, kwik::shader::kTriangleVert, kwik::shader::kTriangleVertSize);
    VkShaderModule frag =
        VulkanContext::createShaderModule(device_, kwik::shader::kTriangleFrag, kwik::shader::kTriangleFragSize);
    if (!vert || !frag) return false;

    VkPipelineShaderStageCreateInfo stages[] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vert, "main"},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, frag, "main"},
    };

    // ── 顶点输入: float2 ──
    VkVertexInputBindingDescription vtxBind{0, 2 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription vtxAttr{0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
    VkPipelineVertexInputStateCreateInfo vtxIn{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vtxIn.vertexBindingDescriptionCount = 1;
    vtxIn.pVertexBindingDescriptions = &vtxBind;
    vtxIn.vertexAttributeDescriptionCount = 1;
    vtxIn.pVertexAttributeDescriptions = &vtxAttr;

    // ── 图元: 三角形列表 ──
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
                                              VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

    VkPipelineViewportStateCreateInfo vp{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};

    // ── 光栅化 ──
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

    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;

    // ── 混合: 预乘 Alpha ──
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

    // ── 动态状态 ──
    // 声明 stencil 动态状态以保持与矩形渲染器的一致性。
    // 三角形管线自身 stencilTestEnable = VK_FALSE（不读写 stencil），
    // 但声明这些动态状态可防止绑定三角形管线后失效化之前设置的
    // stencil compareMask / writeMask / reference 值，
    // 从而避免后续矩形绘制时因状态失效而产生验证报错。
    VkDynamicState dynStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,    // ← 新增
        VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,       // ← 新增
        VK_DYNAMIC_STATE_STENCIL_REFERENCE,        // ← 新增
    };
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 5;                     // 2 → 5
    dyn.pDynamicStates = dynStates;

    // ── Push constants ──
    VkPushConstantRange pcRange{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants)};
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &pcRange;

    if (vkCreatePipelineLayout(device_, &pl, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        vkDestroyShaderModule(device_, vert, nullptr);
        vkDestroyShaderModule(device_, frag, nullptr);
        return false;
    }

    // ── 创建 pipeline ──
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
    pi.pDepthStencilState = &ds;

    VkPipelineCache cache = VK_NULL_HANDLE;
    VkResult res = vkCreateGraphicsPipelines(device_, cache, 1, &pi, nullptr, &pipeline_);
    vkDestroyShaderModule(device_, vert, nullptr);
    vkDestroyShaderModule(device_, frag, nullptr);

    // ── 创建 host-visible 顶点缓冲 ──
    // 用于在 render pass 内通过 memcpy 上传三角形顶点数据
    // 替代 vkCmdUpdateBuffer（不能在 render pass 内调用）
    bufferCapacity_ = kDefaultCapacity;
    if (!VulkanContext::createBuffer(device_, physDevice_, bufferCapacity_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     stagingBuffer_, stagingMemory_)) {
        return false;
    }
    vkMapMemory(device_, stagingMemory_, 0, bufferCapacity_, 0, &mappedData_);

    return res == VK_SUCCESS;
}

void TriangleRenderer::drawTriangles(VkCommandBuffer cmd, VkExtent2D extent, const std::vector<Vec2> &vertices,
                                     const Color &color, float alpha) {
    if (vertices.size() < 3 || (vertices.size() % 3) != 0) return;

    VkDeviceSize dataSize = vertices.size() * sizeof(Vec2);
    // 检测剩余空间是否足够（每帧 vertex buffer 首地址从 writeOffset_ 开始）
    if (writeOffset_ + dataSize > bufferCapacity_) return;

    // ── 写入顶点数据到当前偏移位置 ──
    // 每次 drawTriangles 追加到 staging buffer 的不同偏移，避免帧内多次调用互相覆盖
    memcpy(static_cast<char *>(mappedData_) + writeOffset_, vertices.data(), dataSize);

    // ── Push constants ──
    PushConstants pc;
    pc.r = color.r / 255.0f;
    pc.g = color.g / 255.0f;
    pc.b = color.b / 255.0f;
    pc.a = color.a / 255.0f;
    pc.viewportW = static_cast<float>(extent.width);
    pc.viewportH = static_cast<float>(extent.height);
    pc.opacity = alpha;
    pc._pad0 = 0.0f;
    pc._pad1 = 0.0f;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(PushConstants), &pc);

    // ── 视口（Y 翻转：NDC Y↑ → 屏幕 Y↓）──
    VkViewport vp{0.0f,
                   0.0f,
                  static_cast<float>(extent.width),
                  static_cast<float>(extent.height),
                  0.0f,
                  1.0f};
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkDeviceSize offset = writeOffset_;
    vkCmdBindVertexBuffers(cmd, 0, 1, &stagingBuffer_, &offset);
    vkCmdDraw(cmd, static_cast<uint32_t>(vertices.size()), 1, 0, 0);

    // 写入偏移前移，为下一组顶点腾出空间
    writeOffset_ += dataSize;
}