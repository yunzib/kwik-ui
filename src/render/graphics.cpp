module;

#include <stdint.h>

module kwik.render.graphics;

import std;
import kwik.core.types;
import kwik.render.command;
import kwik.render.backend;
import kwik.render.software_backend;
import kwik.render.vulkan_backend;

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
    currentState_.tx += dx;
    currentState_.ty += dy;
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
    addCommand(ClipRoundedRectCmd{transformed, radius});
}

void Graphics::resetClip() {
    addCommand(ResetClipCmd{});
}

// ============================================================================
// 绘制命令
// ============================================================================

void Graphics::clear(const Color &color) {
    Color transformed = applyOpacity(color);
    addCommand(ClearCmd{transformed});
}

void Graphics::drawRect(const Rect &rect, const Color &color) {
    Rect transformed = transformRect(rect);
    Color transformedColor = applyOpacity(color);
    addCommand(FillRectCmd{transformed, transformedColor});
}

void Graphics::drawRoundedRect(const Rect &rect, float radius, const Color &color) {
    Rect transformed = transformRect(rect);
    Color transformedColor = applyOpacity(color);
    addCommand(FillRoundedRectCmd{transformed, radius, transformedColor});
}

void Graphics::drawRoundedRectStroke(const Rect &rect, float radius, const Color &color, float strokeWidth) {
    Rect transformed = transformRect(rect);
    Color transformedColor = applyOpacity(color);
    addCommand(StrokeRoundedRectCmd{transformed, radius, transformedColor, strokeWidth});
}

void Graphics::drawShadow(const Rect &rect, float radius, const Shadow &shadow) {
    Rect transformed = transformRect(rect);
    Shadow transformedShadow = shadow;
    transformedShadow.color = applyOpacity(shadow.color);
    addCommand(DrawShadowCmd{transformed, radius, transformedShadow});
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

Rect Graphics::transformRect(const Rect &rect) const {
    float x = rect.x * currentState_.sx + currentState_.tx;
    float y = rect.y * currentState_.sy + currentState_.ty;
    float w = rect.width * currentState_.sx;
    float h = rect.height * currentState_.sy;
    return {x, y, w, h};
}

Color Graphics::applyOpacity(const Color &color) const {
    Color result = color;
    result.a = static_cast<uint8_t>(color.a * currentState_.opacity);
    return result;
}
