// ============================================================================
// g3d_gltf.h — glTF 模型加载接口 (fastgltf 封装)
//
// 纯 C 风格接口, 不引入任何 C++ 模块类型, 供 G3D 组件 (模块 TU) 调用。
// 实现体在 src/element/g3d_gltf.cpp (非 module TU, 避免 fastgltf/simdjson
// 与 C++26 import std 冲突)。
// ============================================================================
#pragma once
#include <string>
#include <vector>

// 从 glTF 提取的顶点 (位置 + 法线, 均对象空间)
struct G3DVertex {
    float x, y, z;     // 位置
    float nx, ny, nz;  // 法线
};

// 一个网格 = 一个三角形列表 (索引已展开, 每 3 顶点 = 1 三角形)
struct G3DMeshData {
    std::vector<G3DVertex> vertices;
};

// 解析出的完整模型: 网格池 + 节点层级 (节点经 meshIndex 引用网格)
struct G3DModel {
    std::vector<G3DMeshData> meshes;

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
 * @param out  输出模型 (网格池 + 节点树)
 * @param error 失败时输出原因
 * @return true=成功; false=失败 (error 非空)
 */
bool g3dLoadFromFile(const char *path, G3DModel &out, std::string &error);