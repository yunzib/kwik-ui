module;
#include <vulkan/vulkan.h>
module kwik.render.vulkan_backend;
import kwik.render.vulkan.context;
import kwik.render.vulkan.rect_renderer;
import kwik.render.vulkan.glyph_renderer;
import kwik.render.vulkan.image_renderer;
import kwik.render.vulkan.clip_manager;
import kwik.render.command;
import kwik.core.types;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.core.log;

import std;

VulkanBackend::~VulkanBackend() {
    if (ctx_.device() != VK_NULL_HANDLE) { vkDeviceWaitIdle(ctx_.device()); }
}

// ================================================================
// initialize — 初始化 Context + 子渲染器
// ================================================================
bool VulkanBackend::initialize(void *native) {
    auto t0 = std::chrono::steady_clock::now();
    if (!ctx_.initialize(native)) return false;
    Log::info("[startup] vk_context_init = {} ms",
              std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count());

    deviceCtx_ = DeviceContext{
        .device = ctx_.device(),
        .physicalDevice = ctx_.physicalDevice(),
        .commandPool = ctx_.commandPool(),
        .queue = ctx_.graphicsQueue(),
    };
    auto t1 = std::chrono::steady_clock::now();

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
    if (!image_.create(ctx_.device(), ctx_.physicalDevice(), ctx_.renderPass(), ctx_.vertexBuffer(),
                       ctx_.indexBuffer())) {
        glyph_.destroy();
        rect_.destroy();
        ctx_.shutdown();
        return false;
    }
    if (!triangle_.create(ctx_.device(), ctx_.physicalDevice(), ctx_.renderPass(), ctx_.vertexBuffer(),
                          ctx_.indexBuffer())) {
        Log::error("TriangleRenderer init failed");
        glyph_.destroy();
        rect_.destroy();
        ctx_.shutdown();
        return false;
    }
    if (!mesh_.create(ctx_.device(), ctx_.physicalDevice(), ctx_.renderPass(), ctx_.vertexBuffer(),
                      ctx_.indexBuffer())) {
        Log::error("MeshRenderer init failed");
        glyph_.destroy();
        rect_.destroy();
        ctx_.shutdown();
        return false;
    }
    Log::info("[startup] pipeline_create = {} ms",
              std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t1).count());
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

    if (ctx_.consumeJustRecreated()) {
        Log::info("beginFrame: swapchain recreated -> force full redraw {}x{}", currentToken_->extent.width,
                  currentToken_->extent.height);
        sx = 0;
        sy = 0;
        sw = currentToken_->extent.width;
        sh = currentToken_->extent.height;
    }

    VkRect2D sc{{sx, sy}, {sw, sh}};
    vkCmdSetScissor(currentToken_->commandBuffer, 0, 1, &sc);
    clip_.beginFrame(currentToken_->extent, sc);
    triangle_.resetOffset();
    mesh_.resetOffset();

    ctx_.accumulateDirtyRect(Rect{(float)sx, (float)sy, (float)sw, (float)sh});

    return true;
}

void VulkanBackend::endFrame() {
    drawCalls_ = 0;
    glyph_.uploadPendingGlyphs(deviceCtx_);
    ctx_.endFrame();
}

bool VulkanBackend::present() {
    bool ok = ctx_.present();
    currentToken_.reset();
    return ok;
}

// ================================================================
// 委托 — 全部通过 currentToken_ 获取 Vulkan 句柄，并透传矩阵
// ================================================================
void VulkanBackend::clear(const Color &c) {
    rect_.clear(currentToken_->commandBuffer, currentToken_->extent, c);
}

void VulkanBackend::fillRect(const Rect &r, const Color &c, BlendMode mode, const Transform2D &t) {
    drawCalls_++;
    if (mode == BlendMode::SrcCopy) {
        // 清除路径：r 已是物理坐标（clearRectArea 用 transformRect 烘焙），t 为单位矩阵，忽略
        VkClearAttachment att{};
        att.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        att.clearValue.color = {{0.0f, 0.0f, 0.0f, 0.0f}};
        att.colorAttachment = 0;
        VkClearRect cr{};
        cr.rect = {{static_cast<int32_t>(r.x), static_cast<int32_t>(r.y)},
                   {static_cast<uint32_t>(std::ceil(r.width)), static_cast<uint32_t>(std::ceil(r.height))}};
        cr.baseArrayLayer = 0;
        cr.layerCount = 1;
        vkCmdClearAttachments(currentToken_->commandBuffer, 1, &att, 1, &cr);
        return;
    }
    rect_.fillRect(currentToken_->commandBuffer, currentToken_->extent, r, c, t);
}

void VulkanBackend::fillRoundedRect(const Rect &r, float rad, const Color &c, const Gradient &gradient,
                                    const Transform2D &t) {
    drawCalls_++;
    rect_.fillRoundedRect(currentToken_->commandBuffer, currentToken_->extent, r, rad, c, gradient,
                          clip_.globalAlpha(), t);
}

void VulkanBackend::drawSegment(const DrawSegmentCmd &cmd) {
    drawCalls_++;
    rect_.drawSegment(currentToken_->commandBuffer, currentToken_->extent, cmd.ax, cmd.ay, cmd.bx, cmd.by, cmd.halfW,
                      cmd.color, clip_.globalAlpha(), cmd.t);
}

void VulkanBackend::strokeRoundedRect(const Rect &r, float rad, const Color &c, float sw, const Transform2D &t) {
    drawCalls_++;
    rect_.strokeRoundedRect(currentToken_->commandBuffer, currentToken_->extent, r, rad, c, sw, clip_.globalAlpha(), t);
}

void VulkanBackend::drawShadow(const Rect &r, float rad, const Shadow &s, const Transform2D &t) {
    drawCalls_++;
    rect_.drawShadow(currentToken_->commandBuffer, currentToken_->extent, r, rad, s, clip_.globalAlpha(), t);
}

void VulkanBackend::drawGlyph(const DrawGlyphCmd &cmd) {
    drawCalls_++;
    if (clip_.level() > 0) {
        // stencil 裁剪路径：同样透传矩阵（字形随 View 变换）
        glyph_.drawGlyphClipped(currentToken_->commandBuffer, currentToken_->extent, cmd, clip_.globalAlpha(), cmd.t);
    } else {
        glyph_.drawGlyph(currentToken_->commandBuffer, currentToken_->extent, cmd, clip_.globalAlpha(), cmd.t);
    }
}

void VulkanBackend::drawImage(const DrawImageCmd &cmd) {
    drawCalls_++;
    if (clip_.level() > 0) {
        image_.drawImageClipped(currentToken_->commandBuffer, currentToken_->extent, cmd, clip_.globalAlpha(), cmd.t);
    } else {
        image_.drawImage(currentToken_->commandBuffer, currentToken_->extent, cmd, clip_.globalAlpha(), cmd.t);
    }
}

uint32_t VulkanBackend::createImageTexture(const uint8_t *rgba, uint32_t w, uint32_t h) {
    return image_.createTexture(deviceCtx_, rgba, w, h);
}

void VulkanBackend::destroyImageTexture(uint32_t id) {
    image_.destroyTexture(id);
}

void VulkanBackend::pushClipRoundedRect(const Rect &r, float rad, const Transform2D &t, const Rect &clipRect) {
    if (clip_.level() == 0) {
        vkCmdSetStencilReference(currentToken_->commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 1);
        vkCmdSetStencilWriteMask(currentToken_->commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 0xFF);
        rect_.writeStencilMask(currentToken_->commandBuffer, currentToken_->extent, r, rad, t);   // stencil 仍用逻辑+矩阵
        vkCmdSetStencilCompareMask(currentToken_->commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 0xFF);
        vkCmdSetStencilReference(currentToken_->commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 1);
    }
    clip_.pushClipRoundedRect(currentToken_->commandBuffer, clipRect, rad);   // ← scissor 用物理 AABB
    pushKinds_.push_back(PushKind::Clip);
}

void VulkanBackend::popState() {
    if (pushKinds_.empty()) return;
    PushKind kind = pushKinds_.back();
    pushKinds_.pop_back();
    if (kind == PushKind::Clip) {
        if (currentToken_) {
            clip_.resetClip(currentToken_->commandBuffer);
            if (clip_.level() == 0) { rect_.disableStencilTest(currentToken_->commandBuffer); }
        }
    }
}

void VulkanBackend::fillTriangles(const FillTrianglesCmd &cmd, const AAVertex *vertices) {
    drawCalls_++;
    triangle_.drawTriangles(currentToken_->commandBuffer, currentToken_->extent, vertices, cmd.vertexCount, cmd.color,
                            clip_.globalAlpha(), cmd.t);
}

void VulkanBackend::drawMesh(const DrawMeshCmd &cmd, const Vertex3D *vertices) {
    drawCalls_++;
    mesh_.drawMesh(currentToken_->commandBuffer, currentToken_->extent, cmd.viewport, vertices, cmd.vertexCount,
                   cmd.mvp, cmd.color, cmd.lightDir);
}

