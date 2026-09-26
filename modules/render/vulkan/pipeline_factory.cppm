// ============================================================================
// 模块: kwik.render.vulkan.pipeline_factory
// VkGraphicsPipeline 创建工厂（清单 §四）：14 条管线的公共骨架收口，
// 各 renderer 只声明差异（PipeDesc 8 维），失败回收统一在工厂内。
// 附带 VkPipelineCache 支持（原 11 处创建全部 VK_NULL_HANDLE）。
//
// 不归工厂管：描述符池分配（glyph 固定 1 / image 懒 256 / backdrop resize 8，
// 策略各异各自管理）；管线附属资源（纹理/FBO/staging）。
// ============================================================================
module;
#include <cstdint>
#include <span>
#include <vector>
#include "vulkan/vulkan.h"

export module kwik.render.vulkan.pipeline_factory;

import std;

export struct PipeDesc {
    // ── ① shader 字节对（构建期 spv_to_header 生成的内嵌数组）──
    const uint8_t *vertSpv = nullptr;
    const uint8_t *fragSpv = nullptr;
    size_t vertSize = 0;
    size_t fragSize = 0;

    // ── ② 顶点输入（quad 2×float / GpuVertex 8×float / Vertex3D 6×float）──
    std::span<const VkVertexInputBindingDescription>    bindings;    // 通常 1 条
    std::span<const VkVertexInputAttributeDescription> attrs;

    // ── ③ push constant（全部 VS|FS；0 = 无）──
    uint32_t pushSize = 0;

    // ── ④ 混合形态 ──
    enum class Blend : uint8_t {
        SrcOver,   // 直 alpha 混合（rect/glyph/image/triangle/mesh/backdrop-composite）
        Write,     // 不混合、全通道写入（backdrop blur 离屏两连：全屏覆盖无需求旧值）
        MaskOnly,  // 不混合、不写颜色（rect stencil 掩码写入管线）
    };
    Blend blend = Blend::SrcOver;

    // ── ⑤ depth/stencil 形态 ──
    enum class DS : uint8_t {
        Off,              // 全关（backdrop-blur；triangle 走 Off + 5 动态态防绑定失效）
        StencilTestEqual, // stencil 测试开（EQUAL、不写）——裁剪变体（rect/glyph/image/backdrop-clip）
        StencilWrite,     // stencil 写入（REPLACE、ALWAYS）——rect 掩码写入管线
        DepthWrite,       // depth 测试+写入（LESS_OR_EQUAL）——mesh 3D
    };
    DS depthStencil = DS::Off;

    // ── ⑥ 动态态：true = 5 态（viewport/scissor/stencil ref/compare/write）；
    //    false = 2 态（viewport/scissor，backdrop）──
    bool stencilDynStates = true;

    // ── ⑦ descriptor set layout bindings（空 = 纯 push constant）──
    std::span<const VkDescriptorSetLayoutBinding> descBindings;

    // ── ⑧ 渲染 pass（主 pass / backdrop 离屏 pass）──
    VkRenderPass renderPass = VK_NULL_HANDLE;

    // ── 可选：外部复用（多管线共享一份对象——rect 四条共用 layout 等）──
    VkDescriptorSetLayout externalSetLayout = VK_NULL_HANDLE;   // 空 = 工厂自建
    VkPipelineLayout externalLayout = VK_NULL_HANDLE;           // 传入 = 跳过自建，bundle 原样带回
};

export struct PipeBundle {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;    // externalSetLayout 传入时 == 它
};

/**
 * @brief 创建一条图形管线。内部：shader module（用毕即毁）→ setLayout（可选）
 *    → pipelineLayout → 11 项状态结构（按 DS/blend/动态态枚举填值）→
 *    vkCreateGraphicsPipelines → 任一步失败统一回收已建对象返回空 bundle。
 */
export PipeBundle makePipeline(VkDevice device, VkPipelineCache cache, const PipeDesc &d);
