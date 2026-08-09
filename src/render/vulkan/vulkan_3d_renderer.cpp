/**
 * @file vulkan_3d_renderer.cpp
 * @brief Vulkan 3D 网格渲染器实现
 */

module;
#include <vulkan/vulkan.h>
#include <cstddef>
#include <cstring>
#include "mesh_shaders.h"

module kwik.render.vulkan.mesh_renderer;

import kwik.render.vulkan.context;
import kwik.core.types;
import kwik.render.command;

import std;

namespace {
/**
 * @brief Push constants 布局（必须与 mesh.slang 对齐）
 * 总计 96 bytes: mvp(64) + color(16) + lightDir(12) + pad(4)
 */
struct PushConstants {
    float mvp[16];       // 模型-视图-投影 (offset 0)
    float r, g, b, a;    // color      (offset 64)
    float lx, ly, lz;    // lightDir   (offset 80)
    float _pad;          // pad        (offset 92)
};
static_assert(sizeof(PushConstants) == 96, "PushConstants size mismatch with shader");
}    // anonymous namespace

MeshRenderer::~MeshRenderer() {
    destroy();
}

void MeshRenderer::destroy() {
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

bool MeshRenderer::create(VkDevice device, VkPhysicalDevice physDevice, VkRenderPass renderPass, VkBuffer vertexBuffer,
                          VkBuffer indexBuffer) {
    device_ = device;
    physDevice_ = physDevice;
    vertexBuffer_ = vertexBuffer;
    indexBuffer_ = indexBuffer;

    // ── 加载 shader ──
    VkShaderModule vert =
        VulkanContext::createShaderModule(device_, kwik::shader::kMeshVert, kwik::shader::kMeshVertSize);
    VkShaderModule frag =
        VulkanContext::createShaderModule(device_, kwik::shader::kMeshFrag, kwik::shader::kMeshFragSize);
    if (!vert || !frag) return false;

    VkPipelineShaderStageCreateInfo stages[] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vert, "main"},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, frag, "main"},
    };

    // ── 顶点输入: float3 pos + float3 normal (stride 24) ──
    VkVertexInputBindingDescription vtxBind{0, 6 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription vtxAttrs[2] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof(float)},
    };
    VkPipelineVertexInputStateCreateInfo vtxIn{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vtxIn.vertexBindingDescriptionCount = 1;
    vtxIn.pVertexBindingDescriptions = &vtxBind;
    vtxIn.vertexAttributeDescriptionCount = 2;
    vtxIn.pVertexAttributeDescriptions = vtxAttrs;

    // ── 图元: 三角形列表 ──
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
                                              VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

    VkPipelineViewportStateCreateInfo vp{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};

    // ── 光栅化: 关闭背面剔除 (v1 简化) ──
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

    // ── 深度: test + write, LESS_OR_EQUAL (depth 每帧 CLEAR 为 1.0) ──
    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    ds.stencilTestEnable = VK_FALSE;

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
    // 声明 stencil 动态状态以保持与其他管线一致, 防止绑定本管线后
    // 失效化先前设置的 stencil compareMask / writeMask / reference。
    VkDynamicState dynStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,           VK_DYNAMIC_STATE_SCISSOR,           VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
        VK_DYNAMIC_STATE_STENCIL_WRITE_MASK, VK_DYNAMIC_STATE_STENCIL_REFERENCE,
    };
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 5;
    dyn.pDynamicStates = dynStates;

    // ── Push constants (96B) ──
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

    // ── 创建 host-visible 顶点缓冲 (render pass 内 memcpy 上传) ──
    bufferCapacity_ = kDefaultCapacity;
    if (!VulkanContext::createBuffer(device_, physDevice_, bufferCapacity_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     stagingBuffer_, stagingMemory_)) {
        return false;
    }
    vkMapMemory(device_, stagingMemory_, 0, bufferCapacity_, 0, &mappedData_);

    return res == VK_SUCCESS;
}

/**
 * @brief 绘制 3D 网格
 * @param viewport 元素屏幕矩形：作为 mesh 视口定位 cube 并裁剪
 *
 * 顶点数据来自 DrawList::meshVertices_ 连续内存, 通过 memcpy
 * 一次性写入 host-visible staging buffer。
 */
void MeshRenderer::drawMesh(VkCommandBuffer cmd, VkExtent2D extent, const Rect &viewport, const Vertex3D *vertices,
                            uint32_t vertexCount, const float mvp[16], const Color &color, const float lightDir[3]) {
    if (!vertices || vertexCount < 3 || (vertexCount % 3) != 0) return;

    VkDeviceSize dataSize = vertexCount * sizeof(Vertex3D);
    if (writeOffset_ + dataSize > bufferCapacity_) return;

    memcpy(static_cast<char *>(mappedData_) + writeOffset_, vertices, dataSize);

    PushConstants pc;
    std::memcpy(pc.mvp, mvp, 16 * sizeof(float));
    pc.r = color.r / 255.0f;
    pc.g = color.g / 255.0f;
    pc.b = color.b / 255.0f;
    pc.a = color.a / 255.0f;
    pc.lx = lightDir[0];
    pc.ly = lightDir[1];
    pc.lz = lightDir[2];
    pc._pad = 0.0f;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(PushConstants), &pc);

    // ── 视口: 元素矩形（定位+裁剪），scissor 保持全屏不污染 2D 绘制 ──
    VkViewport vp{viewport.x, viewport.y, std::max(1.0f, viewport.width), std::max(1.0f, viewport.height), 0.0f, 1.0f};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    VkRect2D sc{{0, 0}, extent};
    vkCmdSetScissor(cmd, 0, 1, &sc);

    VkDeviceSize offset = writeOffset_;
    vkCmdBindVertexBuffers(cmd, 0, 1, &stagingBuffer_, &offset);
    vkCmdDraw(cmd, vertexCount, 1, 0, 0);

    writeOffset_ += dataSize;

    // ── 还原全屏 viewport/scissor：mesh 之后的下一条 2D 绘制须回到全屏空间 ──
    VkViewport fullVp{0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f};
    vkCmdSetViewport(cmd, 0, 1, &fullVp);
    VkRect2D fullSc{{0, 0}, extent};
    vkCmdSetScissor(cmd, 0, 1, &fullSc);
}