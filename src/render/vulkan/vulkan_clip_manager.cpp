module;
#include <vulkan/vulkan.h>
#include <cstdint>
#include <cmath>
#include <vector>

module kwik.render.vulkan.clip_manager;
import kwik.core.types;
import std;

// ── scissor 生产收口 ──
// vkCmdSetScissor 契约：矩形必须完整落在渲染区（framebuffer）内，越界矩形按 Vulkan
// 规范是无效操作（未定义行为 → 裁剪失效——陈旧在途快照曾携内容域矩形乘虚而入，
// 代码卡标题条盖住顶栏语言按钮的根因环节之一）。所有 scissor 生产点统一在此对
// 渲染区求交；float 域算交集再转 uint32（int/uint 直接混算会把负值回卷成巨大
// 正数，历史 bug）；floor/ceil 保守展开，圆角/描边的 AA 边缘像素不被裁；
// 空交集收成 0×0（合法空剪，什么都不画）。
static VkRect2D clampToRenderArea(const Rect &r, const VkExtent2D &ext) {
    const float left = std::floor(std::max(r.x, 0.0f));
    const float top = std::floor(std::max(r.y, 0.0f));
    const float right = std::ceil(std::min(r.x + r.width, static_cast<float>(ext.width)));
    const float bottom = std::ceil(std::min(r.y + r.height, static_cast<float>(ext.height)));
    return {{static_cast<int32_t>(left), static_cast<int32_t>(top)},
            {static_cast<uint32_t>(std::max(0.0f, right - left)),
             static_cast<uint32_t>(std::max(0.0f, bottom - top))}};
}

// 两矩形求交（AA 边缘保守展开：floor 左上 / ceil 右下，圆角描边像素不被裁）
static Rect intersectAA(const Rect &a, const Rect &b) {
    const float l = std::floor(std::max(a.x, b.x));
    const float t = std::floor(std::max(a.y, b.y));
    const float r = std::ceil(std::min(a.x + a.width, b.x + b.width));
    const float bo = std::ceil(std::min(a.y + a.height, b.y + b.height));
    return {l, t, std::max(0.0f, r - l), std::max(0.0f, bo - t)};
}

void ClipManager::beginFrame(const VkExtent2D &extent, const VkRect2D &initialScissor) {
    clipStack_.clear();
    clipSaveStack_.clear();
    alphaSaveStack_.clear();
    // 伤害带 scissor 同样收口：backend 只对偏移保证非负，不保证对 extent 求交；
    // int 域用 int64 中转防回卷。空栈回退（resetClip/restoreState/reapplyState）
    // 都直接使用它，必须在源头保证合法。
    const int32_t bandL = std::max(0, initialScissor.offset.x);
    const int32_t bandT = std::max(0, initialScissor.offset.y);
    const int64_t bandR = std::min<int64_t>(bandL + static_cast<int64_t>(initialScissor.extent.width), extent.width);
    const int64_t bandB = std::min<int64_t>(bandT + static_cast<int64_t>(initialScissor.extent.height), extent.height);
    initialScissor_ = {{bandL, bandT},
                       {static_cast<uint32_t>(std::max<int64_t>(0, bandR - bandL)),
                        static_cast<uint32_t>(std::max<int64_t>(0, bandB - bandT))}};
    renderExtent_ = extent;
}

void ClipManager::pushClipRoundedRect(VkCommandBuffer cmd, const Rect &rect, float /*radius*/) {
    // 嵌套求交语义：新 clip 只能「缩小当前可画区」（外层视口/伤害带约束在嵌套链上
    // 永续）。此前为「替换」语义——陈旧在途快照的越界矩形会把伤害带 scissor 整个
    // 顶掉，其内容画进伤害带之外（代码卡盖住顶栏语言按钮的直接机制）。
    // 栈存「已求交生效矩形」，pop 回退即自然恢复父级约束。
    Rect cur = clipStack_.empty()
                   ? Rect{(float)initialScissor_.offset.x, (float)initialScissor_.offset.y,
                          (float)initialScissor_.extent.width, (float)initialScissor_.extent.height}
                   : clipStack_.back();
    Rect eff = intersectAA(cur, rect);
    clipStack_.push_back(eff);
    VkRect2D sc = clampToRenderArea(eff, renderExtent_);
    vkCmdSetScissor(cmd, 0, 1, &sc);
}

void ClipManager::resetClip(VkCommandBuffer cmd) {
    if (!clipStack_.empty()) clipStack_.pop_back();
    VkRect2D sc = clipStack_.empty() ? initialScissor_    // beginFrame 已对渲染区求交
                                     : clampToRenderArea(clipStack_.back(), renderExtent_);
    vkCmdSetScissor(cmd, 0, 1, &sc);
}

void ClipManager::saveState() {
    clipSaveStack_.push_back(clipStack_);
    alphaSaveStack_.push_back(globalAlpha_);
}

void ClipManager::restoreState(VkCommandBuffer cmd) {
    if (!clipSaveStack_.empty()) {
        clipStack_ = std::move(clipSaveStack_.back());
        clipSaveStack_.pop_back();
    }
    if (!alphaSaveStack_.empty()) {
        globalAlpha_ = alphaSaveStack_.back();
        alphaSaveStack_.pop_back();
    }
    VkRect2D sc = clipStack_.empty() ? initialScissor_ : clampToRenderArea(clipStack_.back(), renderExtent_);
    vkCmdSetScissor(cmd, 0, 1, &sc);
}

void ClipManager::setGlobalAlpha(float a) {
    globalAlpha_ = std::clamp(a, 0.0f, 1.0f);
}

std::optional<Rect> ClipManager::currentScissor() const {
    if (clipStack_.empty()) return std::nullopt;
    return clipStack_.back();
}

void ClipManager::reapplyState(VkCommandBuffer cmd) {
    VkRect2D sc = clipStack_.empty() ? initialScissor_ : clampToRenderArea(clipStack_.back(), renderExtent_);
    vkCmdSetScissor(cmd, 0, 1, &sc);
    if (clipStack_.empty()) {
        vkCmdSetStencilReference(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 0);
        vkCmdSetStencilCompareMask(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 0x00);
        vkCmdSetStencilWriteMask(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 0x00);
    } else {
        vkCmdSetStencilReference(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 1);
        vkCmdSetStencilCompareMask(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 0xFF);
        vkCmdSetStencilWriteMask(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 0xFF);
    }
}
