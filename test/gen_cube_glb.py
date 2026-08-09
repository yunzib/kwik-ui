#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# gen_cube_glb.py — 生成测试模型 resources/models/cube.glb
#
# 纯标准库 (struct/json) 手写 GLB 二进制, 无任何第三方依赖:
#   - 立方体: 24 顶点 (每面 4 顶点, 逐面法线) + 36 索引 (UNSIGNED_SHORT)
#   - 单个节点, 携带 TRS (rotation 30° 绕 Y) — 覆盖 g3d_gltf.cpp 的 TRS 路径
# 用法: python resources/models/gen_cube_glb.py
# 输出: 与脚本同目录的 cube.glb
import json
import os
import struct

# 6 个面: (法线, 4 个角点) — 半边长 1, 观察者从外侧看为逆时针
FACES = [
    ((0, 0, 1),  [(-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]),
    ((0, 0, -1), [(-1, 1, -1), (1, 1, -1), (1, -1, -1), (-1, -1, -1)]),
    ((1, 0, 0),  [(1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)]),
    ((-1, 0, 0), [(-1, -1, 1), (-1, 1, 1), (-1, 1, -1), (-1, -1, -1)]),
    ((0, 1, 0),  [(-1, 1, -1), (1, 1, -1), (1, 1, 1), (-1, 1, 1)]),
    ((0, -1, 0), [(-1, -1, 1), (1, -1, 1), (1, -1, -1), (-1, -1, -1)]),
]

positions, normals, indices = [], [], []
for n, corners in FACES:
    base = len(positions)
    for p in corners:
        positions.append(p)
        normals.append(n)
    indices += [base, base + 1, base + 2, base + 2, base + 3, base]

def pack3f(vals):
    return b"".join(struct.pack("<3f", *v) for v in vals)

pos_bin = pack3f(positions)          # 24*12 = 288
nor_bin = pack3f(normals)            # 288
idx_bin = struct.pack("<%dH" % len(indices), *indices)  # 36*2 = 72
bin_data = pos_bin + nor_bin + idx_bin

gltf = {
    "asset": {"version": "2.0", "generator": "kwik-ui gen_cube_glb.py"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [
        {
            "name": "cube",
            "mesh": 0,
            "translation": [0, 0, 0],
            "rotation": [0, 0.2588190451, 0, 0.9659258263],  # 30° 绕 Y (xyzw)
            "scale": [1, 1, 1],
        }
    ],
    "meshes": [
        {
            "name": "cube",
            "primitives": [
                {"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2, "mode": 4}
            ],
        }
    ],
    "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": len(positions),
         "type": "VEC3", "min": [-1, -1, -1], "max": [1, 1, 1]},
        {"bufferView": 1, "componentType": 5126, "count": len(normals), "type": "VEC3"},
        {"bufferView": 2, "componentType": 5123, "count": len(indices), "type": "SCALAR"},
    ],
    "bufferViews": [
        {"buffer": 0, "byteOffset": 0, "byteLength": len(pos_bin)},
        {"buffer": 0, "byteOffset": len(pos_bin), "byteLength": len(nor_bin)},
        {"buffer": 0, "byteOffset": len(pos_bin) + len(nor_bin), "byteLength": len(idx_bin)},
    ],
    "buffers": [{"byteLength": len(bin_data)}],
}

json_bytes = json.dumps(gltf, separators=(",", ":")).encode("utf-8")

def pad4(b, pad_byte):
    return b + bytes([pad_byte]) * ((4 - len(b) % 4) % 4)

json_chunk = pad4(json_bytes, 0x20)   # JSON chunk 用空格对齐
bin_chunk = pad4(bin_data, 0x00)      # BIN chunk 用 0 对齐

length = 12 + 8 + len(json_chunk) + 8 + len(bin_chunk)
glb = struct.pack("<III", 0x46546C67, 2, length)               # "glTF" + version 2
glb += struct.pack("<II", len(json_chunk), 0x4E4F534A) + json_chunk  # "JSON"
glb += struct.pack("<II", len(bin_chunk), 0x004E4942) + bin_chunk    # "BIN\0"

out_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cube.glb")
with open(out_path, "wb") as f:
    f.write(glb)
print("written %s (%d bytes, %d verts, %d indices)" % (out_path, len(glb), len(positions), len(indices)))