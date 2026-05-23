// ============================================================================
// svg_decoder.h — SVG 解码器接口 (nanosvg 封装)
//
// 纯 C 风格接口, 不引入任何 C++ 标准库类型, 供 Image 组件调用。
// 实现体在 svg_decoder.cpp (非 module TU, 避免 C++26 import std 冲突)。
// ============================================================================
#pragma once
#include <cstdint>
#include <string>
#include <vector>
struct SvgImage {
    std::vector<uint8_t> pixels; // 光栅化后的 RGBA8 像素数据
    int width = 0;               // 光栅化宽度 (像素)
    int height = 0;              // 光栅化高度 (像素)
    std::string error;           // 非空表示解析/光栅化失败
};
/**
 * @brief 从磁盘加载 SVG 并光栅化为 RGBA 位图
 *
 * @param path      SVG 文件路径 (系统原生编码)
 * @param targetW   目标光栅化宽度, 0 表示使用 SVG 自身宽或兜底 256
 * @param targetH   目标光栅化高度, 0 表示使用 SVG 自身高或兜底 256
 * @return SvgImage 含 RGBA 像素缓冲区的结果结构体; 失败时 error 字段非空
 */
SvgImage svgLoadFromFile(const char *path, int targetW, int targetH);

SvgImage svgLoadFromMemory(const char *data, size_t dataSize, int targetW, int targetH);