// ============================================================================
// 实现: kwik.render.vulkan.pipeline_factory
// 状态矩阵按 8 维差异枚举填值——固定值（CULL_NONE/FRONT_FACE_CLOCKWISE/
// MS 1x/TOPOLOGY_TRIANGLE_LIST）与 6 个 renderer 手写版逐值一致。
// ============================================================================
module;

#include <cstdint>
#include "vulkan/vulkan.h"

module kwik.render.vulkan.pipeline_factory;

import std;

PipeBundle makePipeline(VkDevice device, VkPipelineCache cache, const PipeDesc &d) {
    PipeBundle out;

    // ── ① shader module（用毕即毁）──
    VkShaderModuleCreateInfo vci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    vci.codeSize = d.vertSize;
    vci.pCode = reinterpret_cast<const uint32_t *>(d.vertSpv);
    VkShaderModule vert = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &vci, nullptr, &vert) != VK_SUCCESS) return {};

    VkShaderModuleCreateInfo fci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    fci.codeSize = d.fragSize;
    fci.pCode = reinterpret_cast<const uint32_t *>(d.fragSpv);
    VkShaderModule frag = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &fci, nullptr, &frag) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vert, nullptr);
        return {};
    }

    // ── ② setLayout/layout（外部 layout 传入则全部复用，跳过自建）──
    if (d.externalLayout != VK_NULL_HANDLE) {
        out.layout = d.externalLayout;
        out.setLayout = d.externalSetLayout;    // 供调用方记录（通常同源）
        // shader module 已建——直接进入管线创建（layout 阶段无工作）
        //（下方 setLayout/layout 分支以 externalSetLayout 已置而跳过）
    }
    if (d.externalLayout == VK_NULL_HANDLE && d.externalSetLayout != VK_NULL_HANDLE) {
        out.setLayout = d.externalSetLayout;
    } else if (d.externalLayout == VK_NULL_HANDLE && !d.descBindings.empty()) {
        VkDescriptorSetLayoutCreateInfo dsl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        dsl.bindingCount = static_cast<uint32_t>(d.descBindings.size());
        dsl.pBindings = d.descBindings.data();
        if (vkCreateDescriptorSetLayout(device, &dsl, nullptr, &out.setLayout) != VK_SUCCESS) {
            vkDestroyShaderModule(device, vert, nullptr);
            vkDestroyShaderModule(device, frag, nullptr);
            return {};
        }
    }

    // ── ③ pipelineLayout ──
    VkPushConstantRange pcr{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, d.pushSize};
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    if (out.setLayout != VK_NULL_HANDLE) {
        plci.setLayoutCount = 1;
        plci.pSetLayouts = &out.setLayout;
    }
    if (d.pushSize > 0) {
        plci.pushConstantRangeCount = 1;
        plci.pPushConstantRanges = &pcr;
    }
    if (d.externalLayout != VK_NULL_HANDLE) {
        // 复用外部 layout，不创建
    } else if (vkCreatePipelineLayout(device, &plci, nullptr, &out.layout) != VK_SUCCESS) {
        if (out.setLayout != VK_NULL_HANDLE && out.setLayout != d.externalSetLayout)
            vkDestroyDescriptorSetLayout(device, out.setLayout, nullptr);
        vkDestroyShaderModule(device, vert, nullptr);
        vkDestroyShaderModule(device, frag, nullptr);
        return {};
    }

    // ── ④ 状态矩阵（按差异枚举填值，固定值与各 renderer 手写版逐值一致）──
    VkPipelineShaderStageCreateInfo stages[2]{
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vert, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, frag, "main", nullptr},
    };

    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = static_cast<uint32_t>(d.bindings.size());
    vi.pVertexBindingDescriptions = d.bindings.data();
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(d.attrs.size());
    vi.pVertexAttributeDescriptions = d.attrs.data();

    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // depth/stencil：主 pass 子通道带 D24S8 附件——禁用态也必须提供该结构
    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    switch (d.depthStencil) {
    case PipeDesc::DS::StencilTestEqual:
        ds.stencilTestEnable = VK_TRUE;
        ds.front.compareOp = VK_COMPARE_OP_EQUAL;
        ds.front.reference = 1;               // 动态态覆盖（VK_DYNAMIC_STATE_STENCIL_REFERENCE）
        ds.front.compareMask = 0xFF;
        ds.front.writeMask = 0;
        ds.back = ds.front;
        break;
    case PipeDesc::DS::StencilWrite:
        // rect 掩码写入管线逐值：仅 passOp=REPLACE，fail/depthFail=KEEP
        ds.stencilTestEnable = VK_TRUE;
        ds.front.failOp = VK_STENCIL_OP_KEEP;
        ds.front.passOp = VK_STENCIL_OP_REPLACE;
        ds.front.depthFailOp = VK_STENCIL_OP_KEEP;
        ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
        ds.front.reference = 1;
        ds.front.compareMask = 0xFF;
        ds.front.writeMask = 0xFF;
        ds.back = ds.front;
        break;
    case PipeDesc::DS::DepthWrite:
        ds.depthTestEnable = VK_TRUE;
        ds.depthWriteEnable = VK_TRUE;
        ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        break;
    case PipeDesc::DS::Off:
    default:
        break;    // 全关
    }

    // 混合形态。手写版此形态为附件零初始化：MaskOnly 不混合且【不写颜色通道】
    // （纯 stencil 用途）colorWriteMask 保持 0；Write 不混合但全通道直写（离屏 blur）
    VkPipelineColorBlendAttachmentState cba{};
    switch (d.blend) {
    case PipeDesc::Blend::SrcOver:
        cba.blendEnable = VK_TRUE;
        cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cba.colorBlendOp = VK_BLEND_OP_ADD;
        cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;    // 6 家手写版逐值一致
        cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cba.alphaBlendOp = VK_BLEND_OP_ADD;
        cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        break;
    case PipeDesc::Blend::Write:
        cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        break;
    case PipeDesc::Blend::MaskOnly:
        break;
    }
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    // 动态态（5 含 stencil 三项 / 2 仅 viewport+scissor）
    VkDynamicState dyn5[5]{
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_STENCIL_REFERENCE, VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
        VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
    };
    VkDynamicState dyn2[2]{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    if (d.stencilDynStates) {
        dy.dynamicStateCount = 5;
        dy.pDynamicStates = dyn5;
    } else {
        dy.dynamicStateCount = 2;
        dy.pDynamicStates = dyn2;
    }

    VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    ci.stageCount = 2;
    ci.pStages = stages;
    ci.pVertexInputState = &vi;
    ci.pInputAssemblyState = &ia;
    ci.pViewportState = &vp;
    ci.pRasterizationState = &rs;
    ci.pMultisampleState = &ms;
    ci.pDepthStencilState = &ds;
    ci.pColorBlendState = &cb;
    ci.pDynamicState = &dy;
    ci.layout = out.layout;
    ci.renderPass = d.renderPass;
    ci.subpass = 0;

    // ── ⑤ 创建 + 统一回收 ──
    if (vkCreateGraphicsPipelines(device, cache, 1, &ci, nullptr, &out.pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(device, out.layout, nullptr);
        if (out.setLayout != VK_NULL_HANDLE && out.setLayout != d.externalSetLayout)
            vkDestroyDescriptorSetLayout(device, out.setLayout, nullptr);
        vkDestroyShaderModule(device, vert, nullptr);
        vkDestroyShaderModule(device, frag, nullptr);
        return {};
    }

    vkDestroyShaderModule(device, vert, nullptr);
    vkDestroyShaderModule(device, frag, nullptr);
    return out;
}
