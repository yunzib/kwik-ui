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
import kwik.render.vulkan.pipeline_factory;    // 管线工厂（§四）
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

bool MeshRenderer::create(VkDevice device, VkPipelineCache cache, VkPhysicalDevice physDevice, VkRenderPass renderPass, VkBuffer vertexBuffer,
                          VkBuffer indexBuffer) {
    device_ = device;
    physDevice_ = physDevice;
    vertexBuffer_ = vertexBuffer;
    indexBuffer_ = indexBuffer;

    // ── 管线经工厂创建（清单 §四）。手写版：depth test+write LESS_OR_EQUAL
    //    （depth 每帧 CLEAR 为 1.0），SrcOver 混合，5 动态态（stencil 三项防绑定
    //    失效，与 triangle 同理），纯 push constant 无描述符——逐值迁移，像素不变。
    static const VkVertexInputBindingDescription vtxBind{0, 6 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    static const VkVertexInputAttributeDescription vtxAttrs[2] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof(float)},
    };
    PipeDesc pd;
    pd.vertSpv = kwik::shader::kMeshVert;  pd.vertSize = kwik::shader::kMeshVertSize;
    pd.fragSpv = kwik::shader::kMeshFrag;  pd.fragSize = kwik::shader::kMeshFragSize;
    pd.bindings = {&vtxBind, 1};
    pd.attrs = {vtxAttrs, 2};
    pd.pushSize = sizeof(PushConstants);
    pd.blend = PipeDesc::Blend::SrcOver;
    pd.depthStencil = PipeDesc::DS::DepthWrite;
    pd.stencilDynStates = true;
    pd.renderPass = renderPass;

    auto b = makePipeline(device_, cache, pd);
    if (b.pipeline == VK_NULL_HANDLE) return false;
    pipeline_ = b.pipeline;
    pipelineLayout_ = b.layout;

    // ── 创建 host-visible 顶点缓冲 (render pass 内 memcpy 上传) ──
    bufferCapacity_ = kDefaultCapacity;
    if (!VulkanContext::createBuffer(device_, physDevice_, bufferCapacity_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     stagingBuffer_, stagingMemory_)) {
        return false;
    }
    vkMapMemory(device_, stagingMemory_, 0, bufferCapacity_, 0, &mappedData_);

    return true;
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