// ============================================================================
// g3d_gltf.cpp — glTF 模型加载实现 (fastgltf v0.9)
//
// 传统头文件/源文件分离 (非 C++ module TU), 仅依赖标准库 + fastgltf。
// 顶点/法线属性按对象空间原样提取, 索引展开为三角形列表。
// ============================================================================
#include <fastgltf/core.hpp>

#include <cmath>
#include <cstdint>
#include <array>
#include <filesystem>
#include <cstdio>

#include "kwik/g3d_gltf.h"

namespace {

// ── 读取一个 Vec3 float 属性 (POSITION / NORMAL) ───────────────
// v0.9 的 Primitive::attributes 是 Attribute{name, accessorIndex} 列表
bool readVec3Attr(const fastgltf::Asset &asset, const fastgltf::Primitive &prim, const char *name,
                  std::vector<std::array<float, 3>> &out) {
    for (const auto &attr : prim.attributes) {
        if (attr.name != name) continue;
        const auto &accessor = asset.accessors[attr.accessorIndex];
        if (accessor.componentType != fastgltf::ComponentType::Float) return false;
        if (accessor.type != fastgltf::AccessorType::Vec3) return false;
        if (!accessor.bufferViewIndex) return false;

        const auto &bv = asset.bufferViews[*accessor.bufferViewIndex];
        const auto &buf = asset.buffers[bv.bufferIndex];
        // LoadExternalBuffers 选项下 buffer 数据存于 sources::Array
        const auto *arr = std::get_if<fastgltf::sources::Array>(&buf.data);
        if (!arr) return false;

        const uint8_t *base =
            reinterpret_cast<const uint8_t *>(arr->bytes.data()) + bv.byteOffset + accessor.byteOffset;
        size_t stride = bv.byteStride ? *bv.byteStride : 3 * sizeof(float);

        out.resize(accessor.count);
        for (size_t i = 0; i < accessor.count; i++) {
            const float *v = reinterpret_cast<const float *>(base + i * stride);
            out[i] = {v[0], v[1], v[2]};
        }
        return true;
    }
    return false;
}

// ── 读取索引 (UNSIGNED_SHORT / UNSIGNED_INT) ─────────────────
bool readIndices(const fastgltf::Asset &asset, const fastgltf::Primitive &prim, std::vector<uint32_t> &out) {
    if (!prim.indicesAccessor) return false;
    const auto &accessor = asset.accessors[*prim.indicesAccessor];
    if (!accessor.bufferViewIndex) return false;

    const auto &bv = asset.bufferViews[*accessor.bufferViewIndex];
    const auto &buf = asset.buffers[bv.bufferIndex];
    const auto *arr = std::get_if<fastgltf::sources::Array>(&buf.data);
    if (!arr) return false;

    const uint8_t *base = reinterpret_cast<const uint8_t *>(arr->bytes.data()) + bv.byteOffset + accessor.byteOffset;
    size_t elem = (accessor.componentType == fastgltf::ComponentType::UnsignedShort) ? 2 : 4;
    size_t stride = bv.byteStride ? *bv.byteStride : elem;

    out.resize(accessor.count);
    for (size_t i = 0; i < accessor.count; i++) {
        if (accessor.componentType == fastgltf::ComponentType::UnsignedShort)
            out[i] = *reinterpret_cast<const uint16_t *>(base + i * stride);
        else if (accessor.componentType == fastgltf::ComponentType::UnsignedInt)
            out[i] = *reinterpret_cast<const uint32_t *>(base + i * stride);
        else
            return false;
    }
    return true;
}

// ── 重算自 start 起每 3 个顶点的平直法线 (模型未提供 NORMAL 时) ──
void recomputeFlatNormals(std::vector<G3DVertex> &v, size_t start) {
    for (size_t i = start; i + 2 < v.size(); i += 3) {
        G3DVertex &a = v[i], &b = v[i + 1], &c = v[i + 2];
        float ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
        float wx = c.x - a.x, wy = c.y - a.y, wz = c.z - a.z;
        float nx = uy * wz - uz * wy;
        float ny = uz * wx - ux * wz;
        float nz = ux * wy - uy * wx;
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len < 1e-9f) continue;
        nx /= len;
        ny /= len;
        nz /= len;
        for (int k = 0; k < 3; k++) {
            v[i + k].nx = nx;
            v[i + k].ny = ny;
            v[i + k].nz = nz;
        }
    }
}

// ── 构建一个网格 (多个 primitive 拼接为一个三角形列表) ─────
void buildMeshVertices(const fastgltf::Asset &asset, const fastgltf::Mesh &mesh, G3DMeshData &out) {
    for (const auto &prim : mesh.primitives) {
        std::vector<std::array<float, 3>> pos, nor;
        if (!readVec3Attr(asset, prim, "POSITION", pos)) continue;
        bool hasNormal = readVec3Attr(asset, prim, "NORMAL", nor);
        if (!hasNormal) nor.assign(pos.size(), std::array<float, 3>{0, 1, 0});

        std::vector<uint32_t> idx;
        bool hasIdx = readIndices(asset, prim, idx);
        size_t start = out.vertices.size();

        auto emit = [&](uint32_t i) {
            G3DVertex v;
            v.x = pos[i][0];
            v.y = pos[i][1];
            v.z = pos[i][2];
            v.nx = nor[i][0];
            v.ny = nor[i][1];
            v.nz = nor[i][2];
            out.vertices.push_back(v);
        };
        if (hasIdx) {
            for (uint32_t i : idx) emit(i);
        } else {
            for (size_t i = 0; i < pos.size(); i++) emit(static_cast<uint32_t>(i));
        }
        if (!hasNormal) recomputeFlatNormals(out.vertices, start);
    }
}

// ── 递归构建节点树 (TRS 变换) ──────────────────────────────
void buildNode(const fastgltf::Asset &asset, const fastgltf::Node &src, G3DModel::G3DNode &dst) {
    dst.name = src.name;
    dst.meshIndex = src.meshIndex ? static_cast<int>(*src.meshIndex) : -1;

    // Options::DecomposeNodeMatrices 下 transform 恒为 TRS
    if (std::holds_alternative<fastgltf::TRS>(src.transform)) {
        const auto &trs = std::get<fastgltf::TRS>(src.transform);
        dst.translation[0] = trs.translation.x();
        dst.translation[1] = trs.translation.y();
        dst.translation[2] = trs.translation.z();
        dst.rotation[0] = trs.rotation.x();
        dst.rotation[1] = trs.rotation.y();
        dst.rotation[2] = trs.rotation.z();
        dst.rotation[3] = trs.rotation.w();
        dst.scale[0] = trs.scale.x();
        dst.scale[1] = trs.scale.y();
        dst.scale[2] = trs.scale.z();
    }
    for (auto childIdx : src.children) {
        dst.children.emplace_back();
        buildNode(asset, asset.nodes[childIdx], dst.children.back());
    }
}

}    // namespace

// ============================================================================
// g3dLoadFromFile — 入口
// ============================================================================
bool g3dLoadFromFile(const char *path, G3DModel &out, std::string &error) {
    std::filesystem::path p(path);

    auto buffer = fastgltf::GltfDataBuffer::FromPath(p);
    if (buffer.error() != fastgltf::Error::None) {
        error = std::string("无法打开文件: ") + path;
        return false;
    }

    fastgltf::Parser parser;
    auto options = fastgltf::Options::LoadExternalBuffers | fastgltf::Options::DecomposeNodeMatrices;
    auto asset = parser.loadGltf(buffer.get(), p.parent_path(), options);
    if (asset.error() != fastgltf::Error::None) {
        error = "glTF 解析失败 (error=" + std::to_string(static_cast<int>(asset.error())) + ")";
        return false;
    }

    std::fprintf(stderr, "[GLTF] meshes=%zu nodes=%zu accessors=%zu buffers=%zu\n", asset.get().meshes.size(),
                 asset.get().nodes.size(), asset.get().accessors.size(), asset.get().buffers.size());

    // ── 网格池 ──
    out.meshes.clear();
    for (const auto &mesh : asset.get().meshes) {
        out.meshes.emplace_back();
        buildMeshVertices(asset.get(), mesh, out.meshes.back());
    }

    // ── 根节点: 收集所有无父节点的节点 ──
    std::vector<char> hasParent(asset.get().nodes.size(), 0);
    for (const auto &node : asset.get().nodes)
        for (auto childIdx : node.children) hasParent[childIdx] = 1;

    out.root = G3DModel::G3DNode{};
    for (size_t i = 0; i < asset.get().nodes.size(); i++) {
        if (!hasParent[i]) {
            out.root.children.emplace_back();
            buildNode(asset.get(), asset.get().nodes[i], out.root.children.back());
        }
    }
    return true;
}