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
    if (!image_.create(ctx_.device(), ctx_.physicalDevice(), ctx_.renderPass(), ctx_.vertexBuffer(),
                       ctx_.indexBuffer())) {
        glyph_.destroy();
        rect_.destroy();
        ctx_.shutdown();
        return false;
    }
    // 创建 triangle 渲染器，传入物理设备用于分配 host-visible 顶点缓冲
    if (!triangle_.create(ctx_.device(), ctx_.physicalDevice(), ctx_.renderPass(), ctx_.vertexBuffer(),
                          ctx_.indexBuffer())) {
        Log::error("TriangleRenderer init failed");
        glyph_.destroy();
        rect_.destroy();
        ctx_.shutdown();
        return false;
    }
    // 创建 3D 网格渲染器 (深度测试管线)
    if (!mesh_.create(ctx_.device(), ctx_.physicalDevice(), ctx_.renderPass(), ctx_.vertexBuffer(),
                      ctx_.indexBuffer())) {
        Log::error("MeshRenderer init failed");
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

    if (ctx_.consumeJustRecreated()) {    // ← 移到声明之后
        Log::info("beginFrame: swapchain recreated -> force full redraw {}x{}", currentToken_->extent.width,
                  currentToken_->extent.height);
        sx = 0;
        sy = 0;
        sw = currentToken_->extent.width;
        sh = currentToken_->extent.height;    // 覆盖 dirtyRect，黑 canvas 整幅重绘
    }

    VkRect2D sc{{sx, sy}, {sw, sh}};
    vkCmdSetScissor(currentToken_->commandBuffer, 0, 1, &sc);
    clip_.beginFrame(currentToken_->extent, sc);
    triangle_.resetOffset();
    mesh_.resetOffset();                       // 每帧重置 3D 顶点写入偏移

    ctx_.accumulateDirtyRect(Rect{
        (float)sx, (float)sy,
        (float)sw, (float)sh
    });

    return true;
}

void VulkanBackend::endFrame() {
    // Log::info("endFrame: drawCalls={}", drawCalls_);    
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
// 委托 — 全部通过 currentToken_ 获取 Vulkan 句柄
// ================================================================
void VulkanBackend::clear(const Color &c) {
    rect_.clear(currentToken_->commandBuffer, currentToken_->extent, c);
}

void VulkanBackend::fillRect(const Rect &r, const Color &c, BlendMode mode) {
    drawCalls_++;
    // ── clearRect 路径：直接清除到透明黑 ──
    if (mode == BlendMode::SrcCopy) {
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
    // ── 正常填充路径（原有逻辑完全不变）──
    rect_.fillRect(currentToken_->commandBuffer, currentToken_->extent, r, c);
}

void VulkanBackend::fillRoundedRect(const Rect &r, float rad, const Color &c) {
    drawCalls_++;
    rect_.fillRoundedRect(currentToken_->commandBuffer, currentToken_->extent, r, rad, c, clip_.globalAlpha());
}

void VulkanBackend::drawSegment(const DrawSegmentCmd &cmd) {
    drawCalls_++;
    rect_.drawSegment(currentToken_->commandBuffer, currentToken_->extent, cmd.ax, cmd.ay, cmd.bx, cmd.by,
                      cmd.halfW, cmd.color, clip_.globalAlpha());
}

void VulkanBackend::strokeRoundedRect(const Rect &r, float rad, const Color &c, float sw) {
    drawCalls_++;
    rect_.strokeRoundedRect(currentToken_->commandBuffer, currentToken_->extent, r, rad, c, sw, clip_.globalAlpha());
}
void VulkanBackend::drawShadow(const Rect &r, float rad, const Shadow &s) {
    drawCalls_++;
    rect_.drawShadow(currentToken_->commandBuffer, currentToken_->extent, r, rad, s, clip_.globalAlpha());
}
void VulkanBackend::drawGlyph(const DrawGlyphCmd &cmd) {
    drawCalls_++;
    if (clip_.level() > 0) {
        glyph_.drawGlyphClipped(currentToken_->commandBuffer, currentToken_->extent, cmd, clip_.globalAlpha());
    } else {
        glyph_.drawGlyph(currentToken_->commandBuffer, currentToken_->extent, cmd, clip_.globalAlpha());
    }
}

void VulkanBackend::drawImage(const DrawImageCmd &cmd) {
    drawCalls_++;
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
    pushKinds_.push_back(PushKind::Clip);
}

void VulkanBackend::setGlobalAlpha(float a) {
    stateStack_.push_back(currentState_);
    currentState_.alpha = a;
    clip_.setGlobalAlpha(a);
    pushKinds_.push_back(PushKind::Alpha);
}

void VulkanBackend::pushTransform(float tx, float ty, float sx, float sy) {
    // 保存当前状态
    stateStack_.push_back(currentState_);
    // 复合变换
    currentState_.tx += tx * currentState_.sx;
    currentState_.ty += ty * currentState_.sy;
    currentState_.sx *= sx;
    currentState_.sy *= sy;
    // 更新 push constant（Vulkan 命令缓冲区写入）
    // 实际需要在 shader 中定义 transform uniform 并在此更新
    pushKinds_.push_back(PushKind::Transform);
}

void VulkanBackend::popState() {
    if (pushKinds_.empty()) return;
    PushKind kind = pushKinds_.back();
    pushKinds_.pop_back();
    switch (kind) {
    case PushKind::Clip:
        if(currentToken_) {
            clip_.resetClip(currentToken_->commandBuffer);    // 还原 scissor（现有）
            // 最外层圆角裁剪退出：关闭 stencil 测试（恢复动态 compareMask=0x00），
            // 否则 EQUAL(ref=1) 持续生效，掩码矩形之外的所有后续绘制被剔除。
            // 与重构前 resetClip() 的行为完全对齐。
            if (clip_.level() == 0) { rect_.disableStencilTest(currentToken_->commandBuffer); }
        }
        break;
    case PushKind::Alpha:
        if (!stateStack_.empty()) {
            currentState_ = stateStack_.back();
            stateStack_.pop_back();
        }
        clip_.setGlobalAlpha(currentState_.alpha);    // ← 同步还原 ClipManager 里的 alpha
        break;
    case PushKind::Transform:
        if (!stateStack_.empty()) {
            currentState_ = stateStack_.back();
            stateStack_.pop_back();
        }
        break;
    }
}

void VulkanBackend::fillTriangles(const FillTrianglesCmd &cmd, const AAVertex *vertices) {
    drawCalls_++;
    triangle_.drawTriangles(currentToken_->commandBuffer, currentToken_->extent, vertices, cmd.vertexCount, cmd.color,
                            clip_.globalAlpha());
}

void VulkanBackend::drawMesh(const DrawMeshCmd &cmd, const Vertex3D *vertices) {
    drawCalls_++;
    mesh_.drawMesh(currentToken_->commandBuffer, currentToken_->extent, cmd.viewport, vertices,
                   cmd.vertexCount, cmd.mvp, cmd.color, cmd.lightDir);
}