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

VulkanBackend::VulkanBackend(int w, int h) : width_(w), height_(h) {
}
VulkanBackend::~VulkanBackend() = default;
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
    if (ctx_.device() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(ctx_.device()); // ← 确保所有 GPU 命令完成
    }
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
bool VulkanBackend::beginFrame() {
    auto &fm = FontManager::instance();
    if (fm.atlasDirty()) {
        glyph_.uploadAtlas(ctx_, fm.atlasData(), fm.atlasWidth(), fm.atlasHeight());
        fm.clearAtlasDirty();
    }
    if (!ctx_.beginFrame()) return false;
    clip_.beginFrame();
    return true;
}
void VulkanBackend::endFrame() {
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
void VulkanBackend::drawGlyph(const DrawGlyphCmd &cmd) {
    glyph_.drawGlyph(ctx_, cmd, clip_.globalAlpha());
}
void VulkanBackend::uploadGlyphAtlas(const uint8_t *d, uint32_t w, uint32_t h) {
    glyph_.uploadAtlas(ctx_, d, w, h);
}
void VulkanBackend::drawImage(const DrawImageCmd &cmd) {
    image_.drawImage(ctx_, cmd, clip_.globalAlpha());
}
uint32_t VulkanBackend::createImageTexture(const uint8_t *rgba, uint32_t w, uint32_t h) {
    return image_.createTexture(ctx_, rgba, w, h);
}
void VulkanBackend::destroyImageTexture(uint32_t id) {
    image_.destroyTexture(id);
}
void VulkanBackend::pushClipRoundedRect(const Rect &r, float rad) {
    clip_.pushClipRoundedRect(ctx_, r, rad);
}
void VulkanBackend::resetClip() {
    clip_.resetClip(ctx_);
}
void VulkanBackend::saveState() {
    clip_.saveState();
}
void VulkanBackend::restoreState() {
    clip_.restoreState(ctx_);
}
void VulkanBackend::setGlobalAlpha(float a) {
    clip_.setGlobalAlpha(a);
}