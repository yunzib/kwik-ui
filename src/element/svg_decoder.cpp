// ============================================================================
// svg_decoder.cpp — nanosvg 实现体 (非 module 翻译单元)
//
// 独立编译，不使用 C++20 modules, 避免 nanosvg 内部实现
// 与 import std 中的 operator new 产生歧义。
// ============================================================================
#include "nanosvg.h"
#include "nanosvgrast.h"
#include "svg_decoder.h"
SvgImage svgLoadFromFile(const char *path, int targetW, int targetH) {
    SvgImage result;
    // ① 解析 SVG 文件为路径表示 (NSVGimage)
    //    "px" = 长度单位, 96.0f = 默认 DPI
    NSVGimage *image = nsvgParseFromFile(path, "px", 96.0f);
    if (!image) {
        result.error = std::string("SVG parse failed: ") + path;
        return result;
    }
    // ② 确定光栅化分辨率
    //    优先使用调用方指定尺寸 → SVG 自身尺寸 → 兜底 256
    int w = (targetW > 0) ? targetW : (image->width > 0.0f) ? static_cast<int>(image->width) : 256;
    int h = (targetH > 0) ? targetH : (image->height > 0.0f) ? static_cast<int>(image->height) : 256;
    // ③ 分配 RGBA 缓冲区
    result.pixels.resize(static_cast<size_t>(w) * h * 4, 0);
    // ④ 创建光栅化器并渲染到 RGBA 缓冲区
    NSVGrasterizer *rast = nsvgCreateRasterizer();
    if (rast) {
        float scaleX = static_cast<float>(w) / image->width;
        float scaleY = static_cast<float>(h) / image->height;
        // 统一缩放 (保持宽高比由 Image 组件的 fit 模式通过纹理采样处理)
        float scale = (scaleX < scaleY) ? scaleX : scaleY;
        nsvgRasterize(rast, image, 0, 0, scale, result.pixels.data(), w, h, w * 4);
        nsvgDeleteRasterizer(rast);
    }
    // ⑤ 清理
    nsvgDelete(image);
    result.width = w;
    result.height = h;
    return result;
}

// ============================================================================
// svgLoadFromMemory — 从内存解析 SVG (处理中文文件名 / 非 ASCII 路径)
//
// 先复制一份可变缓冲区 (nsvgParse 会内联修改输入)
// ============================================================================
SvgImage svgLoadFromMemory(const char *data, size_t dataSize, int targetW, int targetH) {
    SvgImage result;
    // nanosvg 会在解析时修改输入缓冲区, 必须拷贝一份可写副本
    std::vector<char> buffer(data, data + dataSize);
    buffer.push_back('\0');
    NSVGimage *image = nsvgParse(buffer.data(), "px", 96.0f);
    if (!image) {
        result.error = "SVG parse failed (memory)";
        return result;
    }
    int w = (targetW > 0) ? targetW : (image->width > 0.0f) ? static_cast<int>(image->width) : 256;
    int h = (targetH > 0) ? targetH : (image->height > 0.0f) ? static_cast<int>(image->height) : 256;
    result.pixels.resize(static_cast<size_t>(w) * h * 4, 0);
    NSVGrasterizer *rast = nsvgCreateRasterizer();
    if (rast) {
        float scaleX = static_cast<float>(w) / image->width;
        float scaleY = static_cast<float>(h) / image->height;
        float scale = (scaleX < scaleY) ? scaleX : scaleY;
        nsvgRasterize(rast, image, 0, 0, scale, result.pixels.data(), w, h, w * 4);
        nsvgDeleteRasterizer(rast);
    }
    nsvgDelete(image);
    result.width = w;
    result.height = h;
    return result;
}