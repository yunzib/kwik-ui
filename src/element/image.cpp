// ============================================================================
// image.cpp — Image 控件实现
//
// 支持格式: PNG / JPEG / BMP / GIF / ...  (stb_image)
//          SVG                              (nanosvg, 通过 svg_decoder 封装)
// ============================================================================
module;
// import std 和 stb_image.h 中 new 重载冲突，使用传统头文件格式
#include <stdint.h>
#include <cstdint>
#include <string>
#include <vector>
#include <cstddef>

#include <fstream>
#include <filesystem>

#include "stb_image.h"   // 光栅图像解码 (PNG/JPEG/BMP/GIF/...)
#include "svg_decoder.h" // SVG 解码封装 (nanosvg)
module kwik.element.image;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
// ============================================================================
// 辅助函数 — 扩展名判断
// ============================================================================
namespace {
bool isSvgExtension(const std::string &path) {
    if (path.size() < 4) return false;
    auto ext = path.substr(path.size() - 4);
    return ext == ".svg" || ext == ".SVG";
}
} // namespace
// ============================================================================
// loadImage — 根据来源类型分派加载
// ============================================================================
void Image::loadImage() {
    switch (imageProps_.source) {
    case ImageSource::File: loadFromFile(imageProps_.src); break;
    case ImageSource::Buffer: loadFromBuffer(); break;
    case ImageSource::Url: errorMsg_ = "URL image source not yet implemented"; break;
    }
}
// ============================================================================
// loadFromFile — 扩展名分派: .svg → nanosvg, 其余 → stb_image
// ============================================================================
void Image::loadFromFile(const std::string &path) {
    if (path.empty()) {
        errorMsg_ = "empty image path";
        return;
    }
    // ── SVG 分支 ──────────────────────────────────────────────────────────
    if (isSvgExtension(path)) { return loadFromSvg(path); }
    // ── stb_image 分支 (PNG / JPEG / BMP / GIF / ...) ─────────────────────
    int w = 0, h = 0, channels = 0;
    unsigned char *data = stbi_load(path.c_str(), &w, &h, &channels, 4); // 强制 RGBA4
    if (!data) {
        errorMsg_ = std::string("stbi_load failed: ") + stbi_failure_reason();
        return;
    }
    decodedWidth_ = w;
    decodedHeight_ = h;
    size_t byteCount = static_cast<size_t>(w) * h * 4;
    pixels_.assign(data, data + byteCount);
    stbi_image_free(data);
    loaded_ = true;
}
// ============================================================================
// loadFromSvg — nanosvg 矢量解码 → RGBA 光栅化
// ============================================================================
void Image::loadFromSvg(const std::string &path) {
    int targetW = props.width.has_value() ? static_cast<int>(*props.width) : 0;
    int targetH = props.height.has_value() ? static_cast<int>(*props.height) : 0;
    // 用 std::filesystem::u8path 读取文件 (解决 Windows 下中文路径问题)
    std::ifstream file(std::filesystem::path(path), std::ios::binary | std::ios::ate);
    if (!file) {
        errorMsg_ = "cannot open SVG file: " + path;
        return;
    }
    auto fsize = file.tellg();
    file.seekg(0);
    std::string content(static_cast<size_t>(fsize), '\0');
    file.read(content.data(), fsize);
    SvgImage svg = svgLoadFromMemory(content.data(), content.size(), targetW, targetH);
    if (!svg.error.empty()) {
        errorMsg_ = std::move(svg.error);
        return;
    }
    decodedWidth_ = svg.width;
    decodedHeight_ = svg.height;
    pixels_ = std::move(svg.pixels);
    loaded_ = true;
}
// ============================================================================
// loadFromBuffer — 从 ImageProps::data 构造像素
// ============================================================================
void Image::loadFromBuffer() {
    if (imageProps_.data.empty()) {
        errorMsg_ = "empty image buffer";
        return;
    }
    int w = imageProps_.bufferWidth;
    int h = imageProps_.bufferHeight;
    if (w <= 0 || h <= 0) {
        errorMsg_ = "invalid buffer dimensions";
        return;
    }
    size_t expected = static_cast<size_t>(w) * h * 4;
    if (imageProps_.data.size() < expected) {
        errorMsg_ = "buffer size mismatch";
        return;
    }
    decodedWidth_ = w;
    decodedHeight_ = h;
    pixels_ = imageProps_.data;
    loaded_ = true;
}
// ============================================================================
// onMeasure — 基于图像尺寸和 ViewProps 计算期望尺寸
// ============================================================================
Size Image::onMeasure(Constraints constraints) {
    if (!loaded_) return {0, 0};
    float w = 0.0f, h = 0.0f;
    if (props.width.has_value() && props.height.has_value()) {
        w = *props.width;
        h = *props.height;
    } else if (props.width.has_value()) {
        w = *props.width;
        h = w * static_cast<float>(decodedHeight_) / static_cast<float>(decodedWidth_);
    } else if (props.height.has_value()) {
        h = *props.height;
        w = h * static_cast<float>(decodedWidth_) / static_cast<float>(decodedHeight_);
    } else {
        w = static_cast<float>(decodedWidth_);
        h = static_cast<float>(decodedHeight_);
    }
    return constraints.constrain({w, h});
}
// ============================================================================
// onDraw — 纹理延迟上传 + 绘制
// ============================================================================
void Image::onDraw(Graphics &graphics) {
    View::onDraw(graphics);  // ← 绘制 background / shadow / border
    if (!loaded_) return;
    // 兜底延迟上传 (若 preloadImageTextures 未覆盖)
    if (textureId_ == 0 && !pixels_.empty()) uploadTexture();
    if (textureId_ == 0) return;
    float opacity = imageProps_.imageOpacity * props.opacity;
    graphics.drawImage(textureId_, frame, opacity, props.borderRadius);
}

// ============================================================================
// ~Image — 释放 GPU 纹理资源
// ============================================================================
Image::~Image() {
    if (textureId_ != 0) {
        TextureManager::instance().destroyTexture(textureId_);
        textureId_ = 0;
    }
}

// ============================================================================
// uploadTexture — 同步上传 RGBA 像素到 GPU (仅在渲染循环启动前调用, 线程安全)
// ============================================================================
void Image::uploadTexture() {
    if (textureId_ != 0 || pixels_.empty()) return;
    textureId_ = TextureManager::instance().createTexture(pixels_.data(), static_cast<uint32_t>(decodedWidth_),
                                                          static_cast<uint32_t>(decodedHeight_));
    pixels_.clear();
    pixels_.shrink_to_fit();
}