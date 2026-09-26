module;
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <cstddef>
#include "rect_shaders.h"

module kwik.render.vulkan.rect_renderer;
import kwik.render.vulkan.context;
import kwik.render.vulkan.pipeline_factory;    // 管线工厂（§四）
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
    float shadowOffsetX, shadowOffsetY;   // 渐变复用：linear=起点 / radial=中心（相对 rect 左上）
    float shadowBlur;
    float _pad2;
    float viewportW, viewportH;
    float m00, m01, m02, m10, m11, m12;   // ← 2D 变换矩阵
    float gradientVecX, gradientVecY;     // ← 渐变：linear=终点-起点向量 / radial=(半径,0)
};
static_assert(sizeof(PushConstants) == 128, "PushConstants size mismatch");   // 128 = Vulkan maxPushConstantsSize 保证下限
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
bool RectRenderer::create(VkDevice device, VkPipelineCache cache, VkRenderPass renderPass, VkBuffer vertexBuffer, VkBuffer indexBuffer) {
    device_ = device;
    vertexBuffer_ = vertexBuffer;
    indexBuffer_ = indexBuffer;
    // ── 管线经工厂创建（清单 §四）：fill/stroke/shadow 同参三份（drawMode 在
    //    push constant 区分）+ stencil 掩码写入变体（不混合 + passOp=REPLACE）。
    //    四条共用一份 pipelineLayout（首建复用）。
    static const VkVertexInputBindingDescription vtxBind{0, 2 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    static const VkVertexInputAttributeDescription vtxAttr{0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
    PipeDesc pd;
    pd.vertSpv = kwik::shader::kRectVert;  pd.vertSize = kwik::shader::kRectVertSize;
    pd.fragSpv = kwik::shader::kRectFrag;  pd.fragSize = kwik::shader::kRectFragSize;
    pd.bindings = {&vtxBind, 1};
    pd.attrs = {&vtxAttr, 1};
    pd.pushSize = sizeof(PushConstants);
    pd.blend = PipeDesc::Blend::SrcOver;
    pd.depthStencil = PipeDesc::DS::StencilTestEqual;
    pd.stencilDynStates = true;
    pd.renderPass = renderPass;

    auto b0 = makePipeline(device_, cache, pd);
    if (b0.pipeline == VK_NULL_HANDLE) { destroy(); return false; }
    fillPipeline_ = b0.pipeline;
    pipelineLayout_ = b0.layout;

    pd.externalLayout = pipelineLayout_;    // 后三条复用
    auto bs = makePipeline(device_, cache, pd);
    auto bh = makePipeline(device_, cache, pd);
    if (bs.pipeline == VK_NULL_HANDLE || bh.pipeline == VK_NULL_HANDLE) { destroy(); return false; }
    strokePipeline_ = bs.pipeline;
    shadowPipeline_ = bh.pipeline;

    // stencil 掩码写入：不写颜色 + StencilWrite
    pd.blend = PipeDesc::Blend::MaskOnly;
    pd.depthStencil = PipeDesc::DS::StencilWrite;
    auto bn = makePipeline(device_, cache, pd);
    if (bn.pipeline == VK_NULL_HANDLE) { destroy(); return false; }
    stencilPipeline_ = bn.pipeline;

    return true;
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
    fillRoundedRect(cmd, extent, rect, 0, color, Gradient{}, 1.0f, t);
}

void RectRenderer::fillRoundedRect(VkCommandBuffer cmd, VkExtent2D extent, const Rect &rect, float radius,
                                   const Color &color, const Gradient &gradient, float globalAlpha,
                                   const Transform2D &t) {
    // 渐变模式映射：None=0(fill) / Linear=4 / Radial=5，与 rect.slang fragment 分支对应
    uint32_t mode = 0;
    if (gradient.type == GradientType::Linear) mode = 4;
    else if (gradient.type == GradientType::Radial) mode = 5;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fillPipeline_);
    PushConstants pc{};
    pc.topLeftX = rect.x;
    pc.topLeftY = rect.y;
    pc.sizeX    = rect.width;
    pc.sizeY    = rect.height;
    pc.fillR = color.r / 255.f;              // color0（Graphics 已烘焙透明度）
    pc.fillG = color.g / 255.f;
    pc.fillB = color.b / 255.f;
    pc.fillA = color.a / 255.f;
    pc.radius = radius;
    pc.opacity = globalAlpha;
    pc.drawMode = mode;
    // 渐变参数（相对 rect 左上，由 Graphics 换算好）
    pc.shadowOffsetX = gradient.x0;
    pc.shadowOffsetY = gradient.y0;
    pc.gradientVecX  = gradient.x1;
    pc.gradientVecY  = gradient.y1;
    // 第二色复用 borderColor 通道
    pc.borderR = gradient.color1.r / 255.f;
    pc.borderG = gradient.color1.g / 255.f;
    pc.borderB = gradient.color1.b / 255.f;
    pc.borderA = gradient.color1.a / 255.f;
    pc.viewportW = static_cast<float>(extent.width);
    pc.viewportH = static_cast<float>(extent.height);
    pc.m00 = t.m00; pc.m01 = t.m01; pc.m02 = t.m02;
    pc.m10 = t.m10; pc.m11 = t.m11; pc.m12 = t.m12;
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