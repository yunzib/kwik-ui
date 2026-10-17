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
import kwik.render.vulkan.pipeline_factory;    // 管线工厂（§四）
import kwik.core.types;
import kwik.core.path;
import kwik.render.command;
import kwik.core.log;

import std;

namespace {
/**
 * @brief Push constants 布局（必须与 shader 对齐）
 * 总计 128 bytes: color(16) + viewport(8) + opacity(4) + pad(8) + 矩阵(24)
 *                 + 渐变/环参数(68)，对齐 Vulkan 最小保证 128B
 */
struct PushConstants {
    float r, g, b, a;      // color      (offset 0)
    float viewportW;       // viewport   (offset 16)
    float viewportH;       //            (offset 20)
    float opacity;         //            (offset 24)
    float _pad0, _pad1;    // pad        (offset 28)
    float m00, m01, m02, m10, m11, m12;   // 矩阵（36B→60B）
    // ── Sweep 渐变 / SDF 圆环（gradMode==1/2 时启用）──
    float gradMode;        // 0=纯色 1=扫描渐变 2=SDF圆环 (offset 60)
    float gradCx, gradCy;  // 圆心（本地逻辑坐标）    (offset 64)
    float gradA0, gradA1;  // 起止角（弧度）          (offset 72)
    float gc1r, gc1g, gc1b, gc1a;   // 渐变终点色    (offset 80)
    // ── SDF 圆环参数（gradMode==2 时启用）──
    float ringMidR;        // 圆环中径（本地逻辑坐标） (offset 96)
    float ringHalfW;       // 半带宽（本地逻辑坐标）   (offset 100)
    float ringCapMode;     // 0=圆头 1=平头           (offset 104)
    float _pad3, _pad4, _pad5, _pad6, _pad7;         // pad  → 128B
};
static_assert(sizeof(PushConstants) == 128, "PushConstants size mismatch");

/**
 * @brief GPU 顶点布局（与 triangle.slang 对齐）
 * 位置 float2 + 重心坐标 float2 (w=1-u-v) + 对边标志 float3
 */
struct GpuVertex {
    float x, y;          // 位置
    float bu, bv;        // 重心坐标 (w = 1 - bu - bv)
    float h0, h1, h2;    // 三条边的高（flat，三个顶点相同）
    float em;            // 三角形 edgeMask（flat，三个顶点相同）
};
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
    // 清理延迟销毁队列（防泄漏；shutdown 前已 waitIdle，无在飞引用）
    for (auto &entry : pendingStaging_) {
        if (std::get<1>(entry) != VK_NULL_HANDLE) vkFreeMemory(device_, std::get<1>(entry), nullptr);
        if (std::get<0>(entry) != VK_NULL_HANDLE) vkDestroyBuffer(device_, std::get<0>(entry), nullptr);
    }
    pendingStaging_.clear();
}

bool TriangleRenderer::create(VkDevice device, VkPipelineCache cache, VkPhysicalDevice physDevice, VkRenderPass renderPass, VkBuffer vertexBuffer,
                VkBuffer indexBuffer) {
    device_ = device;
    physDevice_ = physDevice;
    vertexBuffer_ = vertexBuffer;
    indexBuffer_ = indexBuffer;

    // ── 管线经工厂创建（清单 §四）。手写版：DS 全关但声明 5 动态态（stencil 三项
    //    防绑定失效——本管线不读写 stencil，声明动态态避免之前设置的
    //    compareMask/writeMask/reference 被失效化引发验证报错），SrcOver 混合，
    //    纯 push constant 无描述符——逐值迁移，像素不变。
    static const VkVertexInputBindingDescription vtxBind{0, sizeof(GpuVertex), VK_VERTEX_INPUT_RATE_VERTEX};
    static const VkVertexInputAttributeDescription vtxAttrs[4] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},                     // 位置
        {1, 0, VK_FORMAT_R32G32_SFLOAT, 2 * sizeof(float)},     // 重心坐标 (u,v)
        {2, 0, VK_FORMAT_R32G32B32_SFLOAT, 4 * sizeof(float)},  // 三条边的高 h0/h1/h2
        {3, 0, VK_FORMAT_R32_SFLOAT, 7 * sizeof(float)},        // edgeMask
    };
    PipeDesc pd;
    pd.vertSpv = kwik::shader::kTriangleVert;  pd.vertSize = kwik::shader::kTriangleVertSize;
    pd.fragSpv = kwik::shader::kTriangleFrag;  pd.fragSize = kwik::shader::kTriangleFragSize;
    pd.bindings = {&vtxBind, 1};
    pd.attrs = {vtxAttrs, 4};
    pd.pushSize = sizeof(PushConstants);
    pd.blend = PipeDesc::Blend::SrcOver;
    pd.depthStencil = PipeDesc::DS::Off;
    pd.stencilDynStates = true;
    pd.renderPass = renderPass;

    auto b = makePipeline(device_, cache, pd);
    if (b.pipeline == VK_NULL_HANDLE) return false;
    pipeline_ = b.pipeline;
    pipelineLayout_ = b.layout;

    // ── 创建 host-visible 顶点缓冲 ──
    // 用于在 render pass 内通过 memcpy 上传三角形顶点数据
    // 替代 vkCmdUpdateBuffer（不能在 render pass 内调用）
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
 * @brief 绘制三角形列表
 *
 * 顶点数据通过 memcpy 一次性写入 host-visible staging buffer，
 * 避免在 render pass 内使用 vkCmdUpdateBuffer。
 *
 * @param vertices    Arena 顶点区的连续 float2 指针
 * @param vertexCount 顶点总数（= 三角形数 × 3，必须 ≥ 3 且为 3 的倍数）
 * @param sweep       非空时启用 Sweep 扫描渐变（push constant 传圆心/角度/终点色）；
 *                    null 时 gradMode=0，走纯色路径，与旧版完全一致
 */
void TriangleRenderer::drawTriangles(VkCommandBuffer cmd, VkExtent2D extent,
                                     const AAVertex *vertices, uint32_t vertexCount,
                                     const Color &color, float alpha, const Transform2D &t,
                                     const SweepGrad *sweep) {
    // 顶点校验：至少 1 个三角形，且数量为 3 的倍数
    if (!vertices || vertexCount < 3 || (vertexCount % 3) != 0) return;

    VkDeviceSize dataSize = vertexCount * sizeof(GpuVertex);
    // 检测本帧槽剩余空间是否足够；不足时动态扩容重建（2× 递增），不再静默丢弃
    if (writeOffset_ + dataSize > curSlotBase_ + slotCapacity()) {
        if (!growStagingBuffer(dataSize)) {
            Log::warn("[triangle] staging 不足且扩容失败，丢弃命令 (需 {}B)", dataSize);
            return;
        }
    }

     // ── 逐顶点展开：位置 + 重心坐标 + 三条边高 + edgeMask ──
    GpuVertex *dst = reinterpret_cast<GpuVertex *>(static_cast<char *>(mappedData_) + writeOffset_);
    for (uint32_t i = 0; i < vertexCount; ++i) {
        dst[i].x = vertices[i].pos.x;
        dst[i].y = vertices[i].pos.y;
        uint32_t k = i % 3;
        dst[i].bu = (k == 0) ? 1.0f : 0.0f;
        dst[i].bv = (k == 1) ? 1.0f : 0.0f;
        dst[i].h0 = vertices[i].h0;
        dst[i].h1 = vertices[i].h1;
        dst[i].h2 = vertices[i].h2;
        dst[i].em = vertices[i].edgeMask;    // 三个顶点相同，插值不变
    }

    // ── Push constants：颜色 + 视口 + 透明度 + 矩阵 + Sweep 渐变 ──
    // 布局与 triangle.slang TrianglePushConstants 严格对齐（96B），
    // 渐变参数用本地逻辑坐标（与 Linear/Radial 渐变同一语义，随矩阵变换）
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
    pc.m00 = t.m00; pc.m01 = t.m01; pc.m02 = t.m02;
    pc.m10 = t.m10; pc.m11 = t.m11; pc.m12 = t.m12;
    // ── Sweep 渐变参数（sweep==nullptr 时 gradMode=0 → 纯色路径，现有调用零影响）──
    pc.gradMode = (sweep != nullptr) ? 1.0f : 0.0f;
    pc.gradCx = sweep ? sweep->cx : 0.0f;
    pc.gradCy = sweep ? sweep->cy : 0.0f;
    pc.gradA0 = sweep ? sweep->a0 : 0.0f;
    pc.gradA1 = sweep ? sweep->a1 : 0.0f;
    pc.gc1r = sweep ? sweep->color1.r / 255.0f : 0.0f;
    pc.gc1g = sweep ? sweep->color1.g / 255.0f : 0.0f;
    pc.gc1b = sweep ? sweep->color1.b / 255.0f : 0.0f;
    pc.gc1a = sweep ? sweep->color1.a / 255.0f : 0.0f;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdPushConstants(cmd, pipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pc);

    // ── 视口（Y 翻转：NDC Y↑ → 屏幕 Y↓）──
    VkViewport vp{0.0f, 0.0f,
                  static_cast<float>(extent.width),
                  static_cast<float>(extent.height),
                  0.0f, 1.0f};
    vkCmdSetViewport(cmd, 0, 1, &vp);

    // ── 绑定 staging buffer 并绘制 ──
    VkDeviceSize offset = writeOffset_;
    vkCmdBindVertexBuffers(cmd, 0, 1, &stagingBuffer_, &offset);
    vkCmdDraw(cmd, vertexCount, 1, 0, 0);

    // 写入偏移前移，为下一组顶点腾出空间
    writeOffset_ += dataSize;
}

/**
 * @brief 绘制 SDF 圆环（UberSDF 同款）
 *
 * 每环仅生成 6 顶点 2 三角形（覆盖整圆包围盒 + AA 外扩），
 * 环带/端帽/渐变/角度裁剪全部由 fragment SDF 完成 →
 * 消除折线描边 miter join 的退化三角形（近共线时 h≈0.001px → 半透明花色接缝）。
 * 顶点 ATTR1（bary 槽）复用为本地坐标，fragment 用插值结果计算 SDF。
 */
void TriangleRenderer::drawRing(VkCommandBuffer cmd, VkExtent2D extent, const FillRingCmd &ring, float alpha) {
    // ── 覆盖整圆包围盒的 quad（外扩 pad 覆盖 AA 过渡带）──
    float R = ring.midR + ring.halfW + ring.pad;
    float x0 = ring.cx - R, y0 = ring.cy - R;
    float x1 = ring.cx + R, y1 = ring.cy + R;

    GpuVertex v[6] = {
        {x0, y0, x0, y0, 0.f, 0.f, 0.f, 0.f},
        {x1, y0, x1, y0, 0.f, 0.f, 0.f, 0.f},
        {x1, y1, x1, y1, 0.f, 0.f, 0.f, 0.f},
        {x0, y0, x0, y0, 0.f, 0.f, 0.f, 0.f},
        {x1, y1, x1, y1, 0.f, 0.f, 0.f, 0.f},
        {x0, y1, x0, y1, 0.f, 0.f, 0.f, 0.f},
    };
    VkDeviceSize dataSize = sizeof(v);
    // 与 drawTriangles 一致：本帧槽不足时动态扩容，不再静默丢弃
    if (writeOffset_ + dataSize > curSlotBase_ + slotCapacity()) {
        if (!growStagingBuffer(dataSize)) {
            Log::warn("[triangle] ring 命令 staging 不足且扩容失败，丢弃");
            return;
        }
    }
    std::memcpy(static_cast<char *>(mappedData_) + writeOffset_, v, dataSize);

    // ── Push constants：gradMode=2 走 SDF 圆环分支 ──
    PushConstants pc;
    pc.r = ring.color0.r / 255.0f;
    pc.g = ring.color0.g / 255.0f;
    pc.b = ring.color0.b / 255.0f;
    pc.a = ring.color0.a / 255.0f;
    pc.viewportW = static_cast<float>(extent.width);
    pc.viewportH = static_cast<float>(extent.height);
    pc.opacity = alpha;
    pc._pad0 = 0.0f;
    pc._pad1 = 0.0f;
    pc.m00 = ring.t.m00; pc.m01 = ring.t.m01; pc.m02 = ring.t.m02;
    pc.m10 = ring.t.m10; pc.m11 = ring.t.m11; pc.m12 = ring.t.m12;
    pc.gradMode = 2.0f;
    pc.gradCx = ring.cx; pc.gradCy = ring.cy;
    pc.gradA0 = ring.a0; pc.gradA1 = ring.a1;
    pc.gc1r = ring.color1.r / 255.0f;
    pc.gc1g = ring.color1.g / 255.0f;
    pc.gc1b = ring.color1.b / 255.0f;
    pc.gc1a = ring.color1.a / 255.0f;
    pc.ringMidR = ring.midR;
    pc.ringHalfW = ring.halfW;
    pc.ringCapMode = ring.roundCap ? 0.0f : 1.0f;
    pc._pad3 = 0.0f; pc._pad4 = 0.0f; pc._pad5 = 0.0f; pc._pad6 = 0.0f; pc._pad7 = 0.0f;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdPushConstants(cmd, pipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pc);

    VkViewport vp{0.0f, 0.0f, static_cast<float>(extent.width),
                  static_cast<float>(extent.height), 0.0f, 1.0f};
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkDeviceSize offset = writeOffset_;
    vkCmdBindVertexBuffers(cmd, 0, 1, &stagingBuffer_, &offset);
    vkCmdDraw(cmd, 6, 1, 0, 0);

    writeOffset_ += dataSize;
}

bool TriangleRenderer::growStagingBuffer(VkDeviceSize required) {
    // 需求按"本帧槽内用量"计：扩容后的单槽容量必须覆盖已写 + 本次需求，
    // 总容量 = 槽需求 × 分槽数（否则新槽容量不足会连续扩容）
    size_t usedInSlot = writeOffset_ - curSlotBase_;
    size_t slotNeed = usedInSlot + static_cast<size_t>(required);
    size_t newCap = std::max(bufferCapacity_ * 2, slotNeed * kFrameSlots);
    VkBuffer newBuf = VK_NULL_HANDLE;
    VkDeviceMemory newMem = VK_NULL_HANDLE;
    void *newMap = nullptr;
    // 重建 host-visible 顶点缓冲（与原创建路径一致）
    if (!VulkanContext::createBuffer(device_, physDevice_, newCap, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     newBuf, newMem)) {
        Log::error("[triangle] staging 扩容创建缓冲失败 ({})", newCap);
        return false;
    }
    if (vkMapMemory(device_, newMem, 0, newCap, 0, &newMap) != VK_SUCCESS) {
        vkFreeMemory(device_, newMem, nullptr);
        vkDestroyBuffer(device_, newBuf, nullptr);
        Log::error("[triangle] staging 扩容映射内存失败 ({})", newCap);
        return false;
    }
    // 保留本帧槽内已写入数据（迁移到新容量同名槽位），槽内偏移保持不变
    size_t newBase = slotBase(curSlot_);
    if (usedInSlot > 0 && mappedData_)
        std::memcpy(static_cast<char *>(newMap) + newBase, static_cast<char *>(mappedData_) + curSlotBase_, usedInSlot);
    // 旧缓冲入延迟销毁队列（倒计时 kFrameSlots 帧）：在飞近 2 帧的命令缓冲
    // 仍引用它，帧首 fence 只等 3 帧前提交——须等引用者全部执行完
    if (stagingBuffer_ != VK_NULL_HANDLE) pendingStaging_.emplace_back(stagingBuffer_, stagingMemory_, kFrameSlots);
    if (mappedData_) vkUnmapMemory(device_, stagingMemory_);
    stagingBuffer_ = newBuf;
    stagingMemory_ = newMem;
    mappedData_ = newMap;
    bufferCapacity_ = newCap;
    curSlotBase_ = newBase;
    writeOffset_ = newBase + usedInSlot;
    Log::warn("[triangle] staging 动态扩容 → {}B (槽需 {}B)", newCap, slotNeed);
    return true;
}

void TriangleRenderer::flushPendingDestroy() {
    // 倒计时归零 = 扩容后第 3 帧首：引用旧缓冲的在飞帧此刻必然已被
    // 对应 fence 等待覆盖（帧 M 首等待覆盖 M-3 提交），销毁安全
    for (size_t i = 0; i < pendingStaging_.size();) {
        if (--std::get<2>(pendingStaging_[i]) == 0) {
            if (std::get<1>(pendingStaging_[i]) != VK_NULL_HANDLE)
                vkFreeMemory(device_, std::get<1>(pendingStaging_[i]), nullptr);
            if (std::get<0>(pendingStaging_[i]) != VK_NULL_HANDLE)
                vkDestroyBuffer(device_, std::get<0>(pendingStaging_[i]), nullptr);
            pendingStaging_.erase(pendingStaging_.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            ++i;
        }
    }
}

