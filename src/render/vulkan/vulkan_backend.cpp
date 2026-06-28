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

VulkanBackend::~VulkanBackend() {
    if (ctx_.device() != VK_NULL_HANDLE) { vkDeviceWaitIdle(ctx_.device()); }
}
// ================================================================
// initialize — 初始化 Context + 子渲染器
// ================================================================
bool VulkanBackend::initialize(void *native) {
    if (!ctx_.initialize(native)) return false;

    deviceCtx_ = DeviceContext{
        .device = ctx_.device(),
        .physicalDevice = ctx_.physicalDevice(),
        .commandPool = ctx_.commandPool(),
        .queue = ctx_.graphicsQueue(),
    };

    if (!rect_.create(ctx_.device(), ctx_.renderPass(), ctx_.vertexBuffer(), ctx_.indexBuffer())) {
        ctx_.shutdown();
        return false;
    }
    if (!glyph_.create(ctx_.device(), ctx_.physicalDevice(), ctx_.commandPool(), ctx_.graphicsQueue(),
                   ctx_.renderPass(), ctx_.vertexBuffer(), ctx_.indexBuffer())) {
        rect_.destroy();
        ctx_.shutdown();
        return false;
    }
    if (!image_.create(ctx_.device(), ctx_.physicalDevice(), ctx_.renderPass(), ctx_.vertexBuffer(), ctx_.indexBuffer())) {
        glyph_.destroy();
        rect_.destroy();
        ctx_.shutdown();
        return false;
    }
    return true;
}
void VulkanBackend::shutdown() {
    if (ctx_.device() != VK_NULL_HANDLE) vkDeviceWaitIdle(ctx_.device());
    image_.destroy();
    glyph_.destroy();
    rect_.destroy();
    ctx_.shutdown();
}
bool VulkanBackend::resize(int w, int h) {
    return ctx_.resize(w, h);
}
// ================================================================
// beginFrame — 获取 FrameToken + 设置裁剪初始状态
// ================================================================
bool VulkanBackend::beginFrame(const Rect &dirtyRect) {
    auto token = ctx_.beginFrame();
    if (!token) return false;
    currentToken_ = std::move(token);

    int32_t sx = std::max(0, static_cast<int32_t>(dirtyRect.x));
    int32_t sy = std::max(0, static_cast<int32_t>(dirtyRect.y));
    uint32_t sw = std::max(1u, static_cast<uint32_t>(std::ceil(dirtyRect.width)));
    uint32_t sh = std::max(1u, static_cast<uint32_t>(std::ceil(dirtyRect.height)));
    VkRect2D sc{{sx, sy}, {sw, sh}};
    vkCmdSetScissor(currentToken_->commandBuffer, 0, 1, &sc);

    clip_.beginFrame(currentToken_->extent, sc);
    return true;
}

void VulkanBackend::endFrame() {
    auto &fm = FontManager::instance();
    glyph_.uploadPendingGlyphs(deviceCtx_, fm);
    ctx_.endFrame();
}

void VulkanBackend::present() {
    ctx_.present();
    currentToken_.reset();
}
// ================================================================
// 委托 — 全部通过 currentToken_ 获取 Vulkan 句柄
// ================================================================
void VulkanBackend::clear(const Color &c) {
    rect_.clear(currentToken_->commandBuffer, currentToken_->extent, c);
}
void VulkanBackend::fillRect(const Rect &r, const Color &c) {
    rect_.fillRect(currentToken_->commandBuffer, currentToken_->extent, r, c);
}
void VulkanBackend::fillRoundedRect(const Rect &r, float rad, const Color &c) {
    rect_.fillRoundedRect(currentToken_->commandBuffer, currentToken_->extent, r, rad, c, clip_.globalAlpha());
}
void VulkanBackend::strokeRoundedRect(const Rect &r, float rad, const Color &c, float sw) {
    rect_.strokeRoundedRect(currentToken_->commandBuffer, currentToken_->extent, r, rad, c, sw, clip_.globalAlpha());
}
void VulkanBackend::drawShadow(const Rect &r, float rad, const Shadow &s) {
    rect_.drawShadow(currentToken_->commandBuffer, currentToken_->extent, r, rad, s, clip_.globalAlpha());
}
void VulkanBackend::drawGlyph(const DrawGlyphCmd &cmd) {
    if (clip_.level() > 0) {
        glyph_.drawGlyphClipped(currentToken_->commandBuffer, currentToken_->extent, cmd, clip_.globalAlpha());
    } else {
        glyph_.drawGlyph(currentToken_->commandBuffer, currentToken_->extent, cmd, clip_.globalAlpha());
    }
}

void VulkanBackend::drawImage(const DrawImageCmd &cmd) {
    if (clip_.level() > 0) {
        image_.drawImageClipped(currentToken_->commandBuffer, currentToken_->extent, cmd, clip_.globalAlpha());
    } else {
        image_.drawImage(currentToken_->commandBuffer, currentToken_->extent, cmd, clip_.globalAlpha());
    }
}
uint32_t VulkanBackend::createImageTexture(const uint8_t *rgba, uint32_t w, uint32_t h) {
    return image_.createTexture(deviceCtx_, rgba, w, h);
}
void VulkanBackend::destroyImageTexture(uint32_t id) {
    image_.destroyTexture(id);
}
void VulkanBackend::pushClipRoundedRect(const Rect &r, float rad) {
    if (clip_.level() == 0) {
        vkCmdSetStencilReference(currentToken_->commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 1);
        vkCmdSetStencilWriteMask(currentToken_->commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 0xFF);
        rect_.writeStencilMask(currentToken_->commandBuffer, currentToken_->extent, r, rad);
        vkCmdSetStencilCompareMask(currentToken_->commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 0xFF);
        vkCmdSetStencilReference(currentToken_->commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 1);
    }
    clip_.pushClipRoundedRect(currentToken_->commandBuffer, r, rad);
}
void VulkanBackend::resetClip() {
    clip_.resetClip(currentToken_->commandBuffer);
    if (clip_.level() == 0) { rect_.disableStencilTest(currentToken_->commandBuffer); }
}
void VulkanBackend::saveState() {
    clip_.saveState();
}
void VulkanBackend::restoreState() {
    clip_.restoreState(currentToken_->commandBuffer);
    if (clip_.level() == 0) { rect_.disableStencilTest(currentToken_->commandBuffer); }
}
void VulkanBackend::setGlobalAlpha(float a) {
    clip_.setGlobalAlpha(a);
}