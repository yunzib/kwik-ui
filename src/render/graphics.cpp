module;

#include <stdint.h>

module kwik.render.graphics;

import std;
import kwik.core.types;
import kwik.render.command;
import kwik.render.backend;
// import kwik.render.software_backend;
import kwik.render.vulkan_backend;
import kwik.render.text.types;
import kwik.render.text.pipeline;

// ============================================================================
// 构造函数和析构函数
// ============================================================================

Graphics::Graphics(BackendType backend, int width, int height) : width_(width), height_(height) {
    // 旧构造函数，仅用于兼容性
    // 实际上不创建后端，因为在新架构中后端由渲染线程管理
    // 记录一个警告或什么也不做
}

Graphics::Graphics(CommandBuffer *commandBuffer) : commandBuffer_(commandBuffer) {
}

Graphics::~Graphics() = default;

Graphics::Graphics(Graphics &&other) noexcept :
    commandBuffer_(other.commandBuffer_), width_(other.width_), height_(other.height_),
    stateStack_(std::move(other.stateStack_)), currentState_(other.currentState_) {
    other.commandBuffer_ = nullptr;
}

Graphics &Graphics::operator=(Graphics &&other) noexcept {
    if (this != &other) {
        commandBuffer_ = other.commandBuffer_;
        width_ = other.width_;
        height_ = other.height_;
        stateStack_ = std::move(other.stateStack_);
        currentState_ = other.currentState_;
        other.commandBuffer_ = nullptr;
    }
    return *this;
}

// ============================================================================
// 命令缓冲区管理
// ============================================================================

void Graphics::setCommandBuffer(CommandBuffer *commandBuffer) {
    commandBuffer_ = commandBuffer;
}

void Graphics::getSize(int *width, int *height) const {
    if (width) *width = width_;
    if (height) *height = height_;
}

bool Graphics::checkCommandBuffer() const {
    return commandBuffer_ != nullptr;
}

void Graphics::addCommand(Command cmd) {
    if (commandBuffer_) { commandBuffer_->add(std::move(cmd)); }
}

// ============================================================================
// 状态管理
// ============================================================================

void Graphics::save() {
    stateStack_.push_back(currentState_);
    addCommand(SaveStateCmd{});
}

void Graphics::restore() {
    if (!stateStack_.empty()) {
        currentState_ = stateStack_.back();
        stateStack_.pop_back();
        addCommand(RestoreStateCmd{});
    }
}

void Graphics::translate(float dx, float dy) {
    currentState_.tx += dx * currentState_.sx;
    currentState_.ty += dy * currentState_.sy;
    addCommand(TranslateCmd{dx, dy});
}

void Graphics::scale(float sx, float sy) {
    currentState_.sx *= sx;
    currentState_.sy *= sy;
    addCommand(ScaleCmd{sx, sy});
}

void Graphics::setOpacity(float opacity) {
    currentState_.opacity = std::clamp(opacity, 0.0f, 1.0f);
    addCommand(SetOpacityCmd{currentState_.opacity});
}

// ============================================================================
// 裁剪命令
// ============================================================================

void Graphics::clipRoundedRect(const Rect &rect, float radius) {
    Rect transformed = transformRect(rect);
    addCommand(ClipRoundedRectCmd{transformed, radius * currentState_.sx});
}

void Graphics::resetClip() {
    addCommand(ResetClipCmd{});
}

// ============================================================================
// 绘制命令
// ============================================================================

void Graphics::clear(const Color &color) {
    addCommand(ClearCmd{color});
}

void Graphics::drawRect(const Rect &rect, const Color &color) {
    Rect transformed = transformRect(rect);
    addCommand(FillRectCmd{transformed, color});
}

void Graphics::drawRoundedRect(const Rect &rect, float radius, const Color &color) {
    Rect transformed = transformRect(rect);
    addCommand(FillRoundedRectCmd{transformed, radius * currentState_.sx, color});
}

void Graphics::drawRoundedRectStroke(const Rect &rect, float radius, const Color &color, float strokeWidth) {
    Rect transformed = transformRect(rect);
    addCommand(StrokeRoundedRectCmd{transformed, radius * currentState_.sx, color, strokeWidth * currentState_.sx});
}

void Graphics::drawShadow(const Rect &rect, float radius, const Shadow &shadow) {
    Rect transformed = transformRect(rect);
    addCommand(DrawShadowCmd{transformed, radius * currentState_.sx, shadow});
}

// ============================================================================
// drawText — deprecated，保留兼容性
// ============================================================================
void Graphics::drawText(const std::string &fontPath, const std::string &text, float fontSize, float x, float y,
                        const Color &color) {
    // 已废弃：请使用 TextRenderCache + drawTextCached
    // 需要全局 TextService 指针才能工作
    // TODO: 删除此方法
}

// ============================================================================
// drawTextCached — 使用缓存的排版结果（带 fontId 支持多字体）
// ============================================================================
void Graphics::drawTextCached(const std::vector<ShapedGlyph> &glyphs, const Color &color) {
    for (auto &g : glyphs) {
        float tx = g.x * currentState_.sx + currentState_.tx;
        float ty = g.y * currentState_.sy + currentState_.ty;
        float tw = g.width * currentState_.sx;
        float th = g.height * currentState_.sy;
        addCommand(DrawGlyphCmd{
            g.fontId,
            g.glyphIndex,
            tx, ty, tw, th,
            g.uvLeft, g.uvTop, g.uvRight, g.uvBottom,
            color
        });
    }
}

void Graphics::drawGlyph(const GlyphDrawData &g) {
    float tx = g.x * currentState_.sx + currentState_.tx;
    float ty = g.y * currentState_.sy + currentState_.ty;
    float tw = g.w * currentState_.sx;
    float th = g.h * currentState_.sy;
    addCommand(DrawGlyphCmd{
        0, 0,
        tx, ty, tw, th,
        g.u0, g.v0, g.u1, g.v1,
        g.color
    });
}

// ============================================================================
// drawImage — 录制 DrawImageCmd
// ============================================================================
void Graphics::drawImage(uint32_t textureId, const Rect &rect, float opacity, float cornerRadius) {
    Rect transformed = transformRect(rect);
    addCommand(DrawImageCmd{textureId, transformed, opacity, cornerRadius});
}

// ============================================================================
// 帧控制命令
// ============================================================================

void Graphics::beginFrame() {
    addCommand(BeginFrameCmd{});
}

void Graphics::endFrame() {
    addCommand(EndFrameCmd{});
}

void Graphics::present() {
    addCommand(PresentCmd{});
}

void Graphics::resize(int width, int height) {
    if (width_ == width && height_ == height) { return; }
    width_ = width;
    height_ = height;
    addCommand(ResizeCmd{width, height});
}

// ============================================================================
// 工具函数
// ============================================================================
// 像素对齐
Rect Graphics::transformRect(const Rect &rect) const {
    float x = std::round(rect.x * currentState_.sx + currentState_.tx);
    float y = std::round(rect.y * currentState_.sy + currentState_.ty);
    float w = std::round(rect.width * currentState_.sx);
    float h = std::round(rect.height * currentState_.sy);
    return {x, y, w, h};
}

Color Graphics::applyOpacity(const Color &color) const {
    Color result = color;
    result.a = static_cast<uint8_t>(color.a * currentState_.opacity);
    return result;
}
