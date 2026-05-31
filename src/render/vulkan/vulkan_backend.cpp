module;
#include <vulkan/vulkan.h>
module kwik.render.vulkan_backend;
import kwik.render.font;
import kwik.render.vulkan.context;
import kwik.render.vulkan.rect_renderer;
import kwik.render.vulkan.glyph_renderer;
import kwik.render.vulkan.image_renderer;
import kwik.render.vulkan.clip_manager;
import kwik.render.command;
import kwik.core.types;

import std;

VulkanBackend::VulkanBackend(int w, int h) : width_(w), height_(h) {
}
VulkanBackend::~VulkanBackend() {
    // ── 确保所有 GPU 命令在子模块销毁前完成 ──
    if (ctx_.device() != VK_NULL_HANDLE) { vkDeviceWaitIdle(ctx_.device()); }
}
// ================================================================
// 初始化 / 关闭
// ================================================================
bool VulkanBackend::initialize(void *native, int w, int h) {
    width_ = w;
    height_ = h;
    if (!ctx_.initialize(native, w, h)) return false;
    if (!rect_.create(ctx_)) {
        ctx_.shutdown();
        return false;
    }
    if (!glyph_.create(ctx_)) {
        rect_.destroy();
        ctx_.shutdown();
        return false;
    }
    if (!image_.create(ctx_)) {
        glyph_.destroy();
        rect_.destroy();
        ctx_.shutdown();
        return false;
    }
    return true;
}
void VulkanBackend::shutdown() {
    // vkDeviceWaitIdle 已由析构函数保证，此处可保留为显式调用场景的防御
    if (ctx_.device() != VK_NULL_HANDLE) { vkDeviceWaitIdle(ctx_.device()); }
    image_.destroy();
    glyph_.destroy();
    rect_.destroy();
    ctx_.shutdown();
}
void VulkanBackend::resize(int w, int h) {
    width_ = w;
    height_ = h;
    ctx_.resize(w, h);
}
// ================================================================
// 帧控制
// ================================================================
bool VulkanBackend::beginFrame(const Rect &dirtyRect) {
    if (!ctx_.beginFrame(dirtyRect)) return false;
    VkRect2D initSc = {
        {std::max(0, (int32_t)dirtyRect.x), std::max(0, (int32_t)dirtyRect.y)},
        {std::max(1u, (uint32_t)std::ceil(dirtyRect.width)), std::max(1u, (uint32_t)std::ceil(dirtyRect.height))}};
    clip_.beginFrame(ctx_.extent(), initSc);
    return true;
}
void VulkanBackend::endFrame() {
    // ─ 所有 DrawGlyphCmd 已录制, 后置 atlas 上传确保中帧烘焙字形可见 ─
    auto &fm = FontManager::instance();
    if (fm.atlasDirty()) {
        glyph_.uploadAtlas(ctx_, fm.atlasData(), fm.atlasWidth(), fm.atlasHeight());
        fm.clearAtlasDirty();
    }
    ctx_.endFrame();
}
void VulkanBackend::present() {
    ctx_.present();
}
// ================================================================
// 委托
// ================================================================
void VulkanBackend::clear(const Color &c) {
    rect_.clear(ctx_, c);
}
void VulkanBackend::fillRect(const Rect &r, const Color &c) {
    rect_.fillRect(ctx_, r, c);
}
void VulkanBackend::fillRoundedRect(const Rect &r, float rad, const Color &c) {
    rect_.fillRoundedRect(ctx_, r, rad, c, clip_.globalAlpha());
}
void VulkanBackend::strokeRoundedRect(const Rect &r, float rad, const Color &c, float sw) {
    rect_.strokeRoundedRect(ctx_, r, rad, c, sw, clip_.globalAlpha());
}
void VulkanBackend::drawShadow(const Rect &r, float rad, const Shadow &s) {
    rect_.drawShadow(ctx_, r, rad, s, clip_.globalAlpha());
}
// ================================================================
// drawGlyph — 根据裁剪状态选择管线
// ================================================================
void VulkanBackend::drawGlyph(const DrawGlyphCmd &cmd) {
    if (clip_.level() > 0) {
        glyph_.drawGlyphClipped(ctx_, cmd, clip_.globalAlpha());
    } else {
        glyph_.drawGlyph(ctx_, cmd, clip_.globalAlpha());
    }
}
void VulkanBackend::uploadGlyphAtlas(const uint8_t *d, uint32_t w, uint32_t h) {
    glyph_.uploadAtlas(ctx_, d, w, h);
}
// ================================================================
// drawImage — 根据裁剪状态选择管线
// ================================================================
void VulkanBackend::drawImage(const DrawImageCmd &cmd) {
    if (clip_.level() > 0) {
        image_.drawImageClipped(ctx_, cmd, clip_.globalAlpha());
    } else {
        image_.drawImage(ctx_, cmd, clip_.globalAlpha());
    }
}
uint32_t VulkanBackend::createImageTexture(const uint8_t *rgba, uint32_t w, uint32_t h) {
    return image_.createTexture(ctx_, rgba, w, h);
}
void VulkanBackend::destroyImageTexture(uint32_t id) {
    image_.destroyTexture(id);
}
// ================================================================
// pushClipRoundedRect — 第一层圆角裁剪写入 stencil mask
// ================================================================
void VulkanBackend::pushClipRoundedRect(const Rect &r, float rad) {
    if (clip_.level() == 0) {
        // ── 仅当尚无活跃裁剪时写入 stencil (单层 stencil MVP) ──
        // 1. 写 stencil=mask: 用 fill shader 将圆角矩形写入 stencil bit=1
        vkCmdSetStencilReference(ctx_.commandBuffer(), VK_STENCIL_FACE_FRONT_AND_BACK, 1);
        vkCmdSetStencilWriteMask(ctx_.commandBuffer(), VK_STENCIL_FACE_FRONT_AND_BACK, 0xFF);
        rect_.writeStencilMask(ctx_, r, rad);
        // 2. 启用 stencil 测试: 后续子元素绘制仅 stencil==1 处通过
        vkCmdSetStencilCompareMask(ctx_.commandBuffer(), VK_STENCIL_FACE_FRONT_AND_BACK, 0xFF);
        vkCmdSetStencilReference(ctx_.commandBuffer(), VK_STENCIL_FACE_FRONT_AND_BACK, 1);
    }
    // 3. 记录裁剪状态 (无论是否为第一层都要调, 保持 save/restore 栈一致)
    clip_.pushClipRoundedRect(ctx_, r, rad);
}
// ================================================================
// resetClip — 最后一层裁剪解除时关闭 stencil 测试
// ================================================================
void VulkanBackend::resetClip() {
    clip_.resetClip(ctx_);
    if (clip_.level() == 0) {
        // ── 裁剪栈为空 → 关闭 stencil 测试 ──
        rect_.disableStencilTest(ctx_);
    }
}
void VulkanBackend::saveState() {
    clip_.saveState();
}
void VulkanBackend::restoreState() {
    clip_.restoreState(ctx_);
    // ── 裁剪栈恢复后若为空 → 关闭 stencil 测试 ──
    if (clip_.level() == 0) { rect_.disableStencilTest(ctx_); }
}
void VulkanBackend::setGlobalAlpha(float a) {
    clip_.setGlobalAlpha(a);
}