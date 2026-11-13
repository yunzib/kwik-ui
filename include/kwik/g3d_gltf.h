// ============================================================================
// g3d_gltf.h — glTF 模型加载接口 (fastgltf 封装)
//
// 纯 C 风格接口, 不引入任何 C++ 模块类型, 供 G3D 组件 (模块 TU) 调用。
// 实现体在 extensions/g3d/impl/g3d_gltf.cpp (非 module TU, 避免 fastgltf/
// simdjson 与 C++26 import std 冲突)。
// ============================================================================
#pragma once
#include <cstdint>
#include <string>
#include <vector>

// 从 glTF 提取的顶点 (位置 + 法线 + UV, 均对象空间/模型 UV)
struct G3DVertex {
    float x, y, z;     // 位置
    float nx, ny, nz;  // 法线
    float u, v;        // 纹理坐标 (无 TEXCOORD_0 时为 0)
};

// 材质最小支持: baseColorFactor + baseColorTexture 指向的图片
struct G3DMaterial {
    bool hasMaterial = false;                                // primitive 是否携带材质索引
    float baseColorFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};    // 无材质默认白
    int imageIndex = -1;                                     // baseColorTexture 图片索引 (-1 = 无贴图)
};

// 一个网格 = 一个三角形列表 (索引已展开, 每 3 顶点 = 1 三角形)
struct G3DMeshData {
    std::vector<G3DVertex> vertices;
    G3DMaterial material;    // 取首个带材质 primitive 的材质 (多 primitive 拼接, 最小支持不细分)
};

// 图片原始编码字节 (PNG/JPG; 由扩展层 stb 解码, 加载器不猜格式)
struct G3DImage {
    std::vector<uint8_t> encoded;
};

// 解析出的完整模型: 网格池 + 节点层级 (节点经 meshIndex 引用网格)
struct G3DModel {
    std::vector<G3DMeshData> meshes;
    std::vector<G3DImage> images;    // glTF images (GLB 内嵌 bufferView / 外部 uri 均已读入)

    // 节点层级 (递归), 携带 TRS 本地变换
    struct G3DNode {
        std::string name;
        int meshIndex = -1;                    // -1 = 无网格
        std::vector<G3DNode> children;
        float translation[3] = {0, 0, 0};
        float rotation[4] = {0, 0, 0, 1};      // 四元数 (xyzw)
        float scale[3] = {1, 1, 1};
    };
    G3DNode root;                              // 伪根: 所有无父节点挂其下
};

/**
 * @brief 从磁盘加载 glTF 模型 (glb/gltf)
 * @param path 文件路径 (系统原生编码)
 * @param out  输出模型 (网格池 + 节点树 + 图片字节)
 * @param error 失败时输出原因
 * @return true=成功; false=失败 (error 非空)
 */
bool g3dLoadFromFile(const char *path, G3DModel &out, std::string &error);

/**
 * @brief 解码图片编码字节为 RGBA (stb_image, PNG/JPG/BMP/...)
 *        实现在传统 TU (g3d_gltf.cpp)——module TU include stb 有 new 冲突
 * @param encoded 编码字节 (G3DImage::encoded)
 * @param size    字节数
 * @param rgba    输出 RGBA 像素 (成功时填充)
 * @param w/h     输出尺寸
 * @return true=成功; false=解码失败 (rgba 空)
 */
bool g3dDecodeImage(const uint8_t *encoded, size_t size, std::vector<uint8_t> &rgba, int &w, int &h);