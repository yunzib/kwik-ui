// ============================================================================
// ternmath.cpp — TernMath 数学库实现 (见 modules/ternmath/ternmath.cppm)
// ============================================================================

module;

#include <cmath>

module TernMath;

import std;

// ═══════════════════════════════════════════════════════════════════════════
// 矩阵乘法
// 列主序: (A×B)[col][row] = Σ_k A[k][row] * B[col][k]
//   A[k][row]  = a.m[k*4+row]
//   B[col][k]  = b.m[col*4+k]
// ═══════════════════════════════════════════════════════════════════════════
Mat4 mul(const Mat4 &a, const Mat4 &b) {
    Mat4 r;
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = sum;
        }
    }
    return r;
}

// ═══════════════════════════════════════════════════════════════════════════
// 变换点
// 列主序: 输出分量 = 矩阵第 row 行 × 齐次列向量 (p, 1)
// ═══════════════════════════════════════════════════════════════════════════
Vec3 transformPoint(const Mat4 &m, const Vec3 &p) {
    return {
        m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12],
        m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13],
        m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14],
    };
}

// ═══════════════════════════════════════════════════════════════════════════
// 平移 / 旋转 / 缩放
// ═══════════════════════════════════════════════════════════════════════════
Mat4 translate(float x, float y, float z) {
    Mat4 r;         // 单位阵
    r.m[12] = x;    // row0col3
    r.m[13] = y;    // row1col3
    r.m[14] = z;    // row2col3
    return r;
}

Mat4 rotateX(float rad) {
    float c = std::cos(rad), s = std::sin(rad);
    Mat4 r;
    r.m[5] = c;
    r.m[6] = s;    // col1/col2 × row1/row2
    r.m[9] = -s;
    r.m[10] = c;
    return r;
}

Mat4 rotateY(float rad) {
    float c = std::cos(rad), s = std::sin(rad);
    Mat4 r;
    r.m[0] = c;
    r.m[2] = -s;    // col0/col2 × row0/row2
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

Mat4 rotateZ(float rad) {
    float c = std::cos(rad), s = std::sin(rad);
    Mat4 r;
    r.m[0] = c;
    r.m[1] = s;    // col0/col1 × row0/row1
    r.m[4] = -s;
    r.m[5] = c;
    return r;
}

Mat4 scale(float sx, float sy, float sz) {
    Mat4 r;
    r.m[0] = sx;
    r.m[5] = sy;
    r.m[10] = sz;
    return r;
}

// ═══════════════════════════════════════════════════════════════════════════
// 透视投影 — 右手系, NDC z ∈ [0,1] (Vulkan)
//   m[0]  = f / aspect        f = 1/tan(fov/2)
//   m[5]  = f
//   m[10] = zFar / (zNear - zFar)
//   m[11] = -1
//   m[14] = zNear*zFar / (zNear - zFar)
// ═══════════════════════════════════════════════════════════════════════════
Mat4 perspective(float fovYRad, float aspect, float zNear, float zFar) {
    float f = 1.0f / std::tan(fovYRad * 0.5f);
    Mat4 r{};    // 全零初始化
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = zFar / (zNear - zFar);
    r.m[11] = -1.0f;
    r.m[14] = (zNear * zFar) / (zNear - zFar);
    return r;
}

// ═══════════════════════════════════════════════════════════════════════════
// 视图矩阵 — 右手系, Y 轴向上 (GLM lookAtRH 语义)
//   f = normalize(center - eye)
//   s = normalize(cross(f, up))
//   u = cross(s, f)
//   旋转 = [s.x  s.y  s.z;  u.x  u.y  u.z;  -f.x -f.y -f.z]
//   平移 = [-dot(s,eye); -dot(u,eye); dot(f,eye)]
// ═══════════════════════════════════════════════════════════════════════════
Mat4 lookAt(const Vec3 &eye, const Vec3 &center, const Vec3 &up) {
    Vec3 f = normalize(center - eye);
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);

    Mat4 r;
    r.m[0] = s.x;
    r.m[4] = s.y;
    r.m[8] = s.z;
    r.m[1] = u.x;
    r.m[5] = u.y;
    r.m[9] = u.z;
    r.m[2] = -f.x;
    r.m[6] = -f.y;
    r.m[10] = -f.z;
    r.m[12] = -dot(s, eye);
    r.m[13] = -dot(u, eye);
    r.m[14] = dot(f, eye);
    return r;
}

// ═══════════════════════════════════════════════════════════════════════════
// 矩阵转置
// 列主序: 转置后 r.m[col*4+row] = m.m[row*4+col]
//   m.m[row*4+col] 是 m 的 (row,col) 元素, 交换后落在 r 的 (col,row) 位置
// 用途: mesh shader 的 OpVectorTimesMatrix 实际计算向量×矩阵(即 M^T·p),
//       上传前把 mvp 转置成 M^T 即可得到正确的 M·p 结果
// ═══════════════════════════════════════════════════════════════════════════
Mat4 transpose(const Mat4 &m) {
    Mat4 r;
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row) r.m[c * 4 + row] = m.m[row * 4 + c];
    return r;
}