/**
 * @file vulkan_3d_renderer.cpp
 * @brief Vulkan 3D 网格渲染器实现
 */

module;
#include <vulkan/vulkan.h>
#include <cstddef>
#include <cstring>
#include "mesh_shaders.h"
#include "mesh_tex_shaders.h"    // 贴图变体 frag（材质最小支持）

module kwik.render.vulkan.mesh_renderer;

import kwik.render.vulkan.context;
import kwik.render.vulkan.pipeline_factory;    // 管线工厂（§四）
import kwik.core.types;
import kwik.core.log;
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

// ── 释放 staging 缓冲（destroy / growStaging 共用；不动管线对象）──
void MeshRenderer::freeStaging() {
    VkBuffer oldVB = stagingBuffer_;
    VkDeviceMemory oldMem = stagingMemory_;
    void *oldMap = mappedData_;
    stagingBuffer_ = VK_NULL_HANDLE;
    stagingMemory_ = VK_NULL_HANDLE;
    mappedData_ = nullptr;
    bufferCapacity_ = 0;
    if (oldMap) vkUnmapMemory(device_, oldMem);
    if (oldMem) vkFreeMemory(device_, oldMem, nullptr);
    if (oldVB) vkDestroyBuffer(device_, oldVB, nullptr);
}

void MeshRenderer::destroy() {
    if (pipeline_ != VK_NULL_HANDLE) { vkDestroyPipeline(device_, pipeline_, nullptr); }
    if (pipelineLayout_ != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr); }
    pipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
    if (texPipeline_ != VK_NULL_HANDLE) { vkDestroyPipeline(device_, texPipeline_, nullptr); }
    if (texPipelineLayout_ != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device_, texPipelineLayout_, nullptr); }
    texPipeline_ = VK_NULL_HANDLE;
    texPipelineLayout_ = VK_NULL_HANDLE;
    freeStaging();
}

// ── 帧边界按需倍增 staging：槽不足不再静默丢网格，扩容到能装下 ──
//   释放旧缓冲前须全设备等闲（在飞 1~2 帧仍可能绑定旧缓冲顶点）；
//   每档容量最多触发一次，达到 kMaxCapacity 后停止（持续告警提示丢弃）
void MeshRenderer::growStaging() {
    size_t newCap = std::min(bufferCapacity_ * 2, kMaxCapacity);    // 先记旧容量（freeStaging 会清零）
    freeStaging();
    vkDeviceWaitIdle(device_);    // 在飞帧可能仍绑定旧缓冲——整块重建须等闲

    VkBuffer newBuf = VK_NULL_HANDLE;
    VkDeviceMemory newMem = VK_NULL_HANDLE;
    void *newMap = nullptr;
    if (VulkanContext::createBuffer(device_, physDevice_, newCap, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, newBuf,
                                    newMem) &&
        vkMapMemory(device_, newMem, 0, newCap, 0, &newMap) == VK_SUCCESS) {
        stagingBuffer_ = newBuf;
        stagingMemory_ = newMem;
        mappedData_ = newMap;
        bufferCapacity_ = newCap;
        Log::info("[Mesh] staging 扩容 → {}KB（单帧槽 ≈{} 顶点）", newCap / 1024, slotCapacity() / sizeof(Vertex3D));
    } else {
        // 扩容失败：保住已建资源语义——句柄已摘，容量 0（drawMesh 槽容量 0 全丢弃+告警）
        if (newBuf) vkDestroyBuffer(device_, newBuf, nullptr);
        if (newMem) vkFreeMemory(device_, newMem, nullptr);
        Log::error("[Mesh] staging 扩容到 {}KB 失败，3D 网格将持续丢弃", newCap / 1024);
    }
}

bool MeshRenderer::create(VkDevice device, VkPipelineCache cache, VkPhysicalDevice physDevice, VkRenderPass renderPass,
                          VkBuffer vertexBuffer, VkBuffer indexBuffer, VkDescriptorSetLayout textureSetLayout) {
    device_ = device;
    physDevice_ = physDevice;
    vertexBuffer_ = vertexBuffer;
    indexBuffer_ = indexBuffer;

    // ── 管线经工厂创建（清单 §四）。手写版：depth test+write LESS_OR_EQUAL
    //    （depth 每帧 CLEAR 为 1.0），SrcOver 混合，5 动态态（stencil 三项防绑定
    //    失效，与 triangle 同理），纯 push constant 无描述符——逐值迁移，像素不变。
    //    顶点输入 3 属性（pos/normal/uv，stride 32）——纯色 frag 不读 uv 合法。
    static const VkVertexInputBindingDescription vtxBind{0, 8 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    static const VkVertexInputAttributeDescription vtxAttrs[3] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof(float)},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, 6 * sizeof(float)},
    };
    PipeDesc pd;
    pd.vertSpv = kwik::shader::kMeshVert;  pd.vertSize = kwik::shader::kMeshVertSize;
    pd.fragSpv = kwik::shader::kMeshFrag;  pd.fragSize = kwik::shader::kMeshFragSize;
    pd.bindings = {&vtxBind, 1};
    pd.attrs = {vtxAttrs, 3};
    pd.pushSize = sizeof(PushConstants);
    pd.blend = PipeDesc::Blend::SrcOver;
    pd.depthStencil = PipeDesc::DS::DepthWrite;
    pd.stencilDynStates = true;
    pd.renderPass = renderPass;

    auto b = makePipeline(device_, cache, pd);
    if (b.pipeline == VK_NULL_HANDLE) return false;
    pipeline_ = b.pipeline;
    pipelineLayout_ = b.layout;

    // ── 贴图变体（材质最小支持）：同顶点输入/push/深度/混合，frag 换采样版；
    //    set0 布局经 externalSetLayout 复用 ImageRenderer 主管线布局——
    //    descSet 与管线布局严格同源，纹理生命周期全归 ImageRenderer ──
    if (textureSetLayout != VK_NULL_HANDLE) {
        PipeDesc tp = pd;
        tp.fragSpv = kwik::shader::kMeshTexFrag;  tp.fragSize = kwik::shader::kMeshTexFragSize;
        tp.externalSetLayout = textureSetLayout;
        auto tb = makePipeline(device_, cache, tp);
        if (tb.pipeline == VK_NULL_HANDLE) return false;
        texPipeline_ = tb.pipeline;
        texPipelineLayout_ = tb.layout;
    }

    // ── 创建 host-visible 顶点缓冲 (render pass 内 memcpy 上传) ──
    bufferCapacity_ = kDefaultCapacity;
    if (!VulkanContext::createBuffer(device_, physDevice_, bufferCapacity_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     stagingBuffer_, stagingMemory_)) {
        return false;
    }
    // 持久映射失败：mappedData_ 无效，后续每帧 memcpy 即野指针写——释放缓冲按 init 失败收场
    if (vkMapMemory(device_, stagingMemory_, 0, bufferCapacity_, 0, &mappedData_) != VK_SUCCESS) {
        vkDestroyBuffer(device_, stagingBuffer_, nullptr);
        stagingBuffer_ = VK_NULL_HANDLE;
        vkFreeMemory(device_, stagingMemory_, nullptr);
        stagingMemory_ = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

/**
 * @brief 绘制 3D 网格
 * @param viewport 元素屏幕矩形：作为 mesh 视口定位 cube 并裁剪
 *
 * 顶点数据来自 DrawList::meshVertices_ 连续内存, 通过 memcpy
 * 一次性写入 host-visible staging buffer。
 * textureSet 非空走贴图管线（uv 采样 × 颜色），空走纯色管线（现状行为）。
 */
void MeshRenderer::drawMesh(VkCommandBuffer cmd, const Rect &viewport, VkRect2D scissor, const Vertex3D *vertices,
                            uint32_t vertexCount, const float mvp[16], const Color &color, const float lightDir[3],
                            VkDescriptorSet textureSet) {
    if (!vertices || vertexCount < 3 || (vertexCount % 3) != 0) return;

    VkDeviceSize dataSize = vertexCount * sizeof(Vertex3D);
    if (writeOffset_ + dataSize > curSlotBase_ + slotCapacity()) {
        // 本帧槽不足：丢弃本网格（不再静默）——帧首 resetOffset 见溢出标志将倍增扩容；
        // 告警按帧节流（同帧多个超限网格只报首个，防刷屏；达上限后持续提示丢弃）
        if (!overflowed_) {
            overflowed_ = true;
            Log::warn("[Mesh] 顶点槽不足，丢弃超限网格（槽容量 ≈{} 顶点/帧，本网格 {} 顶点；下帧自动扩容）",
                      slotCapacity() / sizeof(Vertex3D), vertexCount);
        }
        return;
    }
    if (!mappedData_) return;    // 扩容失败后无缓冲——守卫防野指针写

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

    // ── 管线选择：贴图（set0 = ImageRenderer descSet）优先，回退纯色 ──
    VkPipelineLayout layout = pipelineLayout_;
    if (textureSet != VK_NULL_HANDLE && texPipeline_ != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, texPipeline_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, texPipelineLayout_, 0, 1, &textureSet, 0,
                                nullptr);
        layout = texPipelineLayout_;
    } else {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    }
    vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants),
                       &pc);

    // ── viewport 只做几何定位（NDC→屏幕映射），像素裁剪走 scissor：由后端
    //    传入 viewport ∩ 当前可画域（滚动视口/伤害带）——元素滚出视口即
    //    空剪不画（原恒全屏：docsite 滚动后 3D 内容盖到顶栏）──
    VkViewport vp{viewport.x, viewport.y, std::max(1.0f, viewport.width), std::max(1.0f, viewport.height), 0.0f, 1.0f};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkDeviceSize offset = writeOffset_;
    vkCmdBindVertexBuffers(cmd, 0, 1, &stagingBuffer_, &offset);
    vkCmdDraw(cmd, vertexCount, 1, 0, 0);

    writeOffset_ += dataSize;
    // viewport/scissor 动态态不在此还原——后端 clip_.reapplyState 按回放流
    // 当前裁剪态恢复（原"还原全屏"使紧随其后的 SrcCopy 清屏/2D 绘制逃逸裁剪）
}