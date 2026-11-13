#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# gen_cube_tex_glb.py — 生成带材质贴图的测试模型 test/cube_tex.glb
#
# 在 gen_cube_glb.py 基础上扩展 (材质最小支持的回归资产):
#   - 立方体 24 顶点 + TEXCOORD_0 (每面 (0,0)(1,0)(1,1)(0,1)) + 36 索引
#   - 内嵌 PNG (32x32 四象限棋盘: 左上红/右上绿/左下蓝/右下黄) — 目检验证 UV 方向
#   - material: baseColorFactor(白) + baseColorTexture — 覆盖 g3d_gltf.cpp
#     的 UV/材质/图片路径与 G3D 贴图管线 (mesh_tex 变体)
# 纯标准库 (struct/json/zlib), 无第三方依赖。
# 用法: python test/gen_cube_tex_glb.py   (在仓库根执行亦可, 输出固定到脚本同目录)
import json
import os
import struct
import zlib

# ── 6 个面: (法线, 4 角点, 4 UV) — 半边长 1, 外侧逆时针 ──
FACES = [
    ((0, 0, 1),  [(-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]),
    ((0, 0, -1), [(-1, 1, -1), (1, 1, -1), (1, -1, -1), (-1, -1, -1)]),
    ((1, 0, 0),  [(1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)]),
    ((-1, 0, 0), [(-1, -1, 1), (-1, 1, 1), (-1, 1, -1), (-1, -1, -1)]),
    ((0, 1, 0),  [(-1, 1, -1), (1, 1, -1), (1, 1, 1), (-1, 1, 1)]),
    ((0, -1, 0), [(-1, -1, 1), (1, -1, 1), (1, -1, -1), (-1, -1, -1)]),
]
# 每面 4 顶点 UV: 左下(0,0) 右下(1,0) 右上(1,1) 左上(0,1) — 与角点顺序一致
FACE_UVS = [(0, 0), (1, 0), (1, 1), (0, 1)]

positions, normals, uvs, indices = [], [], [], []
for n, corners in FACES:
    base = len(positions)
    for i, p in enumerate(corners):
        positions.append(p)
        normals.append(n)
        uvs.append(FACE_UVS[i])
    indices += [base, base + 1, base + 2, base + 2, base + 3, base]


def make_png(size=32):
    """32x32 RGBA 四象限棋盘 (红/绿/蓝/黄) — 手写最小 PNG, 目检 UV 方向"""
    half = size // 2
    colors = {
        (0, 0): (200, 60, 60, 255),    # 左上红
        (1, 0): (60, 200, 80, 255),    # 右上绿
        (0, 1): (60, 100, 220, 255),   # 左下蓝
        (1, 1): (230, 200, 60, 255),   # 右下黄
    }
    rows = []
    for y in range(size):
        row = bytearray(b"\x00")    # filter type 0 (None)
        for x in range(size):
            row += bytes(colors[(x >= half, y >= half)])
        rows.append(bytes(row))
    idat = zlib.compress(b"".join(rows), 9)

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))    # 8bit RGBA
    png += chunk(b"IDAT", idat)
    png += chunk(b"IEND", b"")
    return png


png_bytes = make_png()

pos_bin = b"".join(struct.pack("<3f", *v) for v in positions)      # 24*12 = 288
nor_bin = b"".join(struct.pack("<3f", *v) for v in normals)        # 288
uv_bin = b"".join(struct.pack("<2f", *v) for v in uvs)             # 24*8 = 192
idx_bin = struct.pack("<%dH" % len(indices), *indices)             # 36*2 = 72
img_pad = (4 - len(idx_bin) % 4) % 4                                # 图片 bufferView 4B 对齐
img_off = len(pos_bin) + len(nor_bin) + len(uv_bin) + len(idx_bin) + img_pad
bin_data = pos_bin + nor_bin + uv_bin + idx_bin + b"\x00" * img_pad + png_bytes

gltf = {
    "asset": {"version": "2.0", "generator": "kwik-ui gen_cube_tex_glb.py"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [
        {
            "name": "cube_tex",
            "mesh": 0,
            "translation": [0, 0, 0],
            "rotation": [0, 0, 0, 1],
            "scale": [1, 1, 1],
        }
    ],
    "meshes": [
        {
            "name": "cube_tex",
            "primitives": [
                {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3,
                 "material": 0, "mode": 4}
            ],
        }
    ],
    "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": len(positions),
         "type": "VEC3", "min": [-1, -1, -1], "max": [1, 1, 1]},
        {"bufferView": 1, "componentType": 5126, "count": len(normals), "type": "VEC3"},
        {"bufferView": 2, "componentType": 5126, "count": len(uvs), "type": "VEC2"},
        {"bufferView": 3, "componentType": 5123, "count": len(indices), "type": "SCALAR"},
    ],
    "bufferViews": [
        {"buffer": 0, "byteOffset": 0, "byteLength": len(pos_bin)},
        {"buffer": 0, "byteOffset": len(pos_bin), "byteLength": len(nor_bin)},
        {"buffer": 0, "byteOffset": len(pos_bin) + len(nor_bin), "byteLength": len(uv_bin)},
        {"buffer": 0, "byteOffset": len(pos_bin) + len(nor_bin) + len(uv_bin), "byteLength": len(idx_bin)},
        {"buffer": 0, "byteOffset": img_off, "byteLength": len(png_bytes)},    # GLB 内嵌 PNG
    ],
    "images": [{"bufferView": 4, "mimeType": "image/png"}],
    "textures": [{"source": 0}],
    "materials": [
        {"pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 1], "baseColorTexture": {"index": 0}}}
    ],
    "buffers": [{"byteLength": len(bin_data)}],
}

json_bytes = json.dumps(gltf, separators=(",", ":")).encode("utf-8")


def pad4(b, pad_byte):
    return b + bytes([pad_byte]) * ((4 - len(b) % 4) % 4)


json_chunk = pad4(json_bytes, 0x20)
bin_chunk = pad4(bin_data, 0x00)

length = 12 + 8 + len(json_chunk) + 8 + len(bin_chunk)
glb = struct.pack("<III", 0x46546C67, 2, length)                          # "glTF" + version 2
glb += struct.pack("<II", len(json_chunk), 0x4E4F534A) + json_chunk        # "JSON"
glb += struct.pack("<II", len(bin_chunk), 0x004E4942) + bin_chunk          # "BIN\0"

out_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cube_tex.glb")
with open(out_path, "wb") as f:
    f.write(glb)
print("written %s (%d bytes, %d verts, %d indices, png %d bytes)" %
      (out_path, len(glb), len(positions), len(indices), len(png_bytes)))
