module;

#include <stdint.h>

module kwik.render.software_backend;

import std;
import kwik.core.types;

SoftwareBackend::SoftwareBackend() = default;

SoftwareBackend::SoftwareBackend(int width, int height) : width_(width), height_(height) {
    pixelBuffer_.resize(width * height);
}

SoftwareBackend::~SoftwareBackend() {
    shutdown();
}

bool SoftwareBackend::initialize(void * /*nativeHandle*/, int width, int height) {
    width_ = width;
    height_ = height;
    pixelBuffer_.resize(width * height);
    return true;
}

void SoftwareBackend::shutdown() {
    pixelBuffer_.clear();
}

void SoftwareBackend::resize(int width, int height) {
    if (width_ == width && height_ == height) { return; }
    width_ = width;
    height_ = height;
    pixelBuffer_.resize(width * height);
}

bool SoftwareBackend::beginFrame() {
    // 软件渲染不需要特殊处理
    return true;
}

void SoftwareBackend::endFrame() {
    // 软件渲染不需要特殊处理
}

void SoftwareBackend::present() {
    // 软件渲染需要将像素缓冲区复制到窗口，这里暂不实现
}

void SoftwareBackend::setGlobalAlpha(float alpha) {
    globalAlpha_ = std::clamp(alpha, 0.0f, 1.0f);
}

void SoftwareBackend::pushClipRoundedRect(const Rect &rect, float radius) {
    // 暂不实现裁剪
}

void SoftwareBackend::resetClip() {
    // 暂不实现裁剪
}

void SoftwareBackend::clear(const Color &color) {
    uint32_t argb = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
    std::fill(pixelBuffer_.begin(), pixelBuffer_.end(), argb);
}

void SoftwareBackend::fillRect(const Rect &rect, const Color &color) {
    // 简单矩形填充，不考虑透明度、裁剪和变换
    int x1 = static_cast<int>(rect.x);
    int y1 = static_cast<int>(rect.y);
    int x2 = static_cast<int>(rect.x + rect.width);
    int y2 = static_cast<int>(rect.y + rect.height);
    x1 = std::max(0, x1);
    y1 = std::max(0, y1);
    x2 = std::min(width_, x2);
    y2 = std::min(height_, y2);
    uint32_t argb = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
    for (int y = y1; y < y2; ++y) {
        for (int x = x1; x < x2; ++x) { pixelBuffer_[y * width_ + x] = argb; }
    }
}

void SoftwareBackend::fillRoundedRect(const Rect &rect, float radius, const Color &color) {
    // 暂实现为普通矩形
    fillRect(rect, color);
}

void SoftwareBackend::strokeRoundedRect(const Rect &rect, float radius, const Color &color, float strokeWidth) {
    // 暂不实现
}

void SoftwareBackend::drawShadow(const Rect &rect, float radius, const Shadow &shadow) {
    // 暂不实现
}
