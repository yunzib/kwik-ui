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
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.backend;         // RenderBackend（纹理按本树后端路由）
import kwik.render.texture_manager;
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
    errorMsg_.clear();    // 失败重装载：清除上一轮错误，防过期误报
    switch (imageProps_.source) {
    case ImageSource::File: loadFromFile(imageProps_.src); break;
    case ImageSource::Buffer: loadFromBuffer(); break;
    case ImageSource::Url: errorMsg_ = "URL image source not yet implemented"; break;
    }
    if (!errorMsg_.empty()) fireError();    // 17 并批②：装载失败 → onError 出口
}
void Image::fireError() {
    if (errorMsg_.empty()) return;
    if (handlers.onError) {
        handlers.onError(errorMsg_);
        errorPending_ = false;
    } else {
        errorPending_ = true;    // 构造期槽未接线（attachJsHandlers 在构造后）——首帧补发
    }
}
// ============================================================================
// applyImageProps — reconcile 属性覆盖：内容标识变更才重新加载解码
// （reconcile 随每次 State 变更触发，无条件重载会对每张图反复磁盘解码）
// ============================================================================
void Image::applyImageProps(ImageProps ip) {
    bool reload;
    if (ip.source != imageProps_.source) {
        reload = true;
    } else {
        switch (ip.source) {
        case ImageSource::File: reload = ip.src != imageProps_.src; break;
        case ImageSource::Buffer:
            reload = ip.bufferWidth != imageProps_.bufferWidth || ip.bufferHeight != imageProps_.bufferHeight
                     || ip.data != imageProps_.data;
            break;
        case ImageSource::Url: reload = ip.src != imageProps_.src; break;
        default: reload = false; break;
        }
    }
    imageProps_ = std::move(ip);
    if (reload) {
        // 17 并批①：内容更换须换纹——旧 textureId_ 不入队销毁并复位的话，
        // onDraw 的 textureId_ 非零守卫使新解码像素永不入队，画面永久停在旧图。
        // destroyTexture 走延迟释放通道（在飞帧引用安全），UI 线程不等待
        if (textureId_ != 0) {
            TextureManager::instance().destroyTexture(
                static_cast<RenderBackend *>(treeService(View::kSvcRenderBackend)), textureId_);
            textureId_ = 0;
        }
        uploadConfirmed_ = false;
        uploadRetries_ = 0;
        loadImage();
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
    svgLoaded_ = true;    // 20 号连带：SVG 源（显式尺寸变更重光栅化判据）
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
// resolveFit — ImageFit 求解（纯函数）：把 iw×ih 图像入 frame 容器
//   Fill     拉伸满幅（历史语义，不变）
//   Contain  等比缩至完全可见，容器内居中（可能留空）
//   Cover    等比扩至完全覆盖（UV 窗口居中裁长轴），dst=frame
//   None     原始尺寸；框含得下居中（UV 满），框小则 1:1 居中裁剪窗口
// 零维输入退回 Fill 等价解（防御）。
// ============================================================================
ImageFitSolution Image::resolveFit(ImageFit fit, Rect frame, float iw, float ih) {
    ImageFitSolution s;
    if (frame.width <= 0 || frame.height <= 0 || iw <= 0 || ih <= 0) {
        s.dst = frame;
        return s;    // 零维防御：等价 Fill（满幅 UV）
    }
    switch (fit) {
    case ImageFit::Contain: {
        float scale = std::min(frame.width / iw, frame.height / ih);
        s.dst.width = iw * scale;
        s.dst.height = ih * scale;
        s.dst.x = frame.x + (frame.width - s.dst.width) * 0.5f;
        s.dst.y = frame.y + (frame.height - s.dst.height) * 0.5f;
        break;    // UV 满幅
    }
    case ImageFit::Cover: {
        s.dst = frame;
        float window = (iw / ih > frame.width / frame.height)    // 图更宽 → 裁横轴
                           ? (frame.width / frame.height) / (iw / ih)
                           : (frame.height / frame.width) / (ih / iw);
        if (window < 1.0f) {    // 全轴满幅时不动（图纵横比恰等）
            float inset = (1.0f - window) * 0.5f;
            if (iw / ih > frame.width / frame.height) {
                s.u0 = inset;
                s.u1 = 1.0f - inset;
            } else {
                s.v0 = inset;
                s.v1 = 1.0f - inset;
            }
        }
        break;
    }
    case ImageFit::None: {
        // 缩放因子恒 1：dst = 原始尺寸在容器内居中（可整体溢出，超框
        // 部分由绘制端 clip(frame) 裁除——与 CSS replaced content 一致）
        s.dst.x = frame.x + (frame.width - iw) * 0.5f;
        s.dst.y = frame.y + (frame.height - ih) * 0.5f;
        s.dst.width = iw;
        s.dst.height = ih;    // UV 满幅
        break;
    }
    case ImageFit::Fill:
        s.dst = frame;
        break;    // 满幅 UV（默认值）
    }
    return s;
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
    if (errorPending_ && handlers.onError) {
        // 17 并批②：构造期装载失败的挂起错误在此补发（槽构造后才接线）
        errorPending_ = false;
        handlers.onError(errorMsg_);
    }
    if (!loaded_) return;
    // 兜底延迟上传 (若 preloadImageTextures 未覆盖)；17 号：上传失败复位后
    // 由 textureId_==0 同一入口重试（源像素于确认落地前保留）
    if (textureId_ == 0) uploadTexture();
    else confirmUpload();    // 异步上传回执：落地释放像素；失败重试或弃传
    if (textureId_ == 0) return;
    float opacity = imageProps_.imageOpacity * props.opacity;
    // 20 号：fit 语义落地——按模式求解目标矩形 + 源 UV 窗口；None 允许
    // dst 溢出 frame（原始尺寸居中），超框部分由圆角裁剪包住（CSS
    // replaced content 裁剪语义；Fill/Contain/Cover 无溢出不必裁剪）
    auto sol = resolveFit(imageProps_.fit, frame, static_cast<float>(decodedWidth_),
                          static_cast<float>(decodedHeight_));
    if (imageProps_.fit == ImageFit::None) {
        graphics.clipRoundedRect(frame, props.borderRadius);
        graphics.drawImageUv(textureId_, sol.dst, sol.u0, sol.v0, sol.u1, sol.v1, opacity,
                             props.borderRadius);
        graphics.resetClip();
    } else {
        graphics.drawImageUv(textureId_, sol.dst, sol.u0, sol.v0, sol.u1, sol.v1, opacity,
                             props.borderRadius);
    }
}

// ============================================================================
// onLayout — SVG 显式尺寸运行期变更重光栅化（20 号连带）
// ============================================================================
void Image::onLayout() {
    View::onLayout();
    if (!svgLoaded_) return;
    if (!props.width.has_value() || !props.height.has_value()) return;    // auto 尺寸不重光栅化（防布局刷写）
    int tw = static_cast<int>(*props.width);
    int th = static_cast<int>(*props.height);
    if (tw <= 0 || th <= 0) return;
    if (decodedWidth_ == tw && decodedHeight_ == th) return;    // 尺寸未变（光栅后帧相等即停，无回环）
    int beforeW = decodedWidth_, beforeH = decodedHeight_;
    loadFromSvg(imageProps_.src);
    if (loaded_) {
        // 尺寸实质变化才有效率收益，防御空转（解码失败保持旧像素不折腾）
        if (decodedWidth_ == beforeW && decodedHeight_ == beforeH) return;
        // 光栅已随宽度/分尺寸变化：纹理须换——旧纹入队销毁 + 重置上传态
        if (textureId_ != 0) {
            TextureManager::instance().destroyTexture(
                static_cast<RenderBackend *>(treeService(View::kSvcRenderBackend)), textureId_);
            textureId_ = 0;
        }
        uploadConfirmed_ = false;
        uploadRetries_ = 0;
    }
}

// ============================================================================
// ~Image — 释放 GPU 纹理资源
// ============================================================================
Image::~Image() {
    if (textureId_ != 0) {
        TextureManager::instance().destroyTexture(
            static_cast<RenderBackend *>(treeService(View::kSvcRenderBackend)), textureId_);
        textureId_ = 0;
    }
}

// ============================================================================
// uploadTexture — 经资源通道入队纹理创建（任意时机线程安全；GPU 上传由
// 渲染线程帧首批量执行，返回的 id 立即可用于绘制引用）
//
// 17 号：入队后源像素保留至落地回执确认（失败重试需源数据）——
// 原实现入队即 clear：渲染线程上传失败 id 永不入 textures_（对上层
// 不可见），且源像素已丢 → 永久白块不可重试。
// ============================================================================
void Image::uploadTexture() {
    if (textureId_ != 0 || pixels_.empty()) return;
    textureId_ = TextureManager::instance().createTexture(
        static_cast<RenderBackend *>(treeService(View::kSvcRenderBackend)),
        pixels_.data(), static_cast<uint32_t>(decodedWidth_),
                                                          static_cast<uint32_t>(decodedHeight_));
    if (textureId_ == 0) {
        auto *backend = static_cast<RenderBackend *>(treeService(View::kSvcRenderBackend));
        if (!backend) return;    // 服务槽未接线（早于 runtime 接线的首帧）——留待后续帧重试
        // 同步拒收：未注册域 / 尺寸非法——重试无意义，弃传报错（并批② 出口）
        failUpload("texture creation rejected (backend domain or invalid size)");
        return;
    }
    // 异步通道已入队：像素保留，confirmUpload 收回执
}

// ============================================================================
// confirmUpload — 拉取异步上传回执（每帧 onDraw 于 textureId_ 非零时调用）
//   Landed → 置落地标记 + 释放源像素（上传成功后源数据使命完成）
//   Failed → textureId_ 复位 0，下一帧重新入队重试；超限弃传（防超大纹理
//            等永久性失败每帧刷写渲染线程）
//   Pending → 在途，继续等
// ============================================================================
void Image::confirmUpload() {
    if (textureId_ == 0 || uploadConfirmed_) return;
    auto st = TextureManager::instance().pollTextureResult(
        static_cast<RenderBackend *>(treeService(View::kSvcRenderBackend)), textureId_);
    switch (st) {
    case RenderBackend::TextureUploadStatus::Landed:
        uploadConfirmed_ = true;
        pixels_.clear();
        pixels_.shrink_to_fit();
        break;
    case RenderBackend::TextureUploadStatus::Failed:
        textureId_ = 0;
        if (++uploadRetries_ > kMaxUploadRetries) {
            failUpload("GPU texture upload failed after retries");
        }
        break;
    case RenderBackend::TextureUploadStatus::Pending:
        break;
    }
}

// ============================================================================
// failUpload — 上传终态失败收场：textureId_ 复位 + 错误置底 + onError（一次）
// pixels_ 一并释放（弃传后源数据再无用途；用户换 src 重装载时按需重建）
// ============================================================================
void Image::failUpload(const std::string &msg) {
    textureId_ = 0;
    errorMsg_ = msg;
    if (handlers.onError) handlers.onError(msg);
    pixels_.clear();
    pixels_.shrink_to_fit();
}