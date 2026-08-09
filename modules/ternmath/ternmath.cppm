/**
 * @file ternmath.cppm
 * @brief TernMath 轻量 3D 数学库 (Vec3 / Mat4)
 *
 * 约定:
 *   - 列主序 (column-major), 与 GLM/Vulkan 一致: m[col*4+row]
 *   - 右手系, Y 轴向上 (与 glTF 场景坐标一致)
 *   - 透视投影输出 NDC 深度 z ∈ [0, 1] (Vulkan 约定)
 *   - 视图/投影矩阵与 OpenGL 语义一致 (y-up);
 *     渲染器通过负高度 viewport 完成 clip→NDC 的 y 翻转 (见 vulkan_3d_renderer)
 *
 * 作为 C++20 模块提供, 使用方 `import TernMath;` 引入。
 * 不再使用命名空间 — 类型 (Vec3/Mat4) 与自由函数均为模块导出符号。
 * 仅依赖标准库。
 */

module;                       // 全局模块片段

#include <cmath>

export module TernMath;

import std;

/**
 * @brief 三维向量
 */
export struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    Vec3() = default;
    Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    Vec3 operator+(const Vec3 &o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3 &o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
};

/**
 * @brief 点积
 * @param a 向量 a
 * @param b 向量 b
 * @return a·b
 */
export inline float dot(const Vec3 &a, const Vec3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/**
 * @brief 叉积 (右手系)
 * @param a 向量 a
 * @param b 向量 b
 * @return a×b
 */
export inline Vec3 cross(const Vec3 &a, const Vec3 &b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

/**
 * @brief 归一化 (零向量返回自身, 避免除零)
 * @param v 输入向量
 * @return 单位向量
 */
export inline Vec3 normalize(const Vec3 &v) {
    float len = std::sqrt(dot(v, v));
    if (len < 1e-9f) return v;
    return v * (1.0f / len);
}

/**
 * @brief 4×4 列主序矩阵
 */
export struct Mat4 {
    /// 列主序: m[col * 4 + row], 默认单位阵
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    /**
     * @brief 构造单位阵
     * @return 单位矩阵
     */
    static Mat4 identity() { return Mat4{}; }

    /**
     * @brief 从 16 元素 (列主序) 数组构造
     * @param v 16 元素数组
     * @return 矩阵
     */
    static Mat4 fromArray(const float v[16]) {
        Mat4 r;
        for (int i = 0; i < 16; ++i) r.m[i] = v[i];
        return r;
    }

    /**
     * @brief 复制到 16 元素 (列主序) 数组 — 供 DrawMeshCmd::mvp 使用
     * @param out 输出数组
     */
    void toArray(float out[16]) const {
        for (int i = 0; i < 16; ++i) out[i] = m[i];
    }
};

// ── 运算 (声明, 实现在 src/ternmath/ternmath.cpp) ──

/**
 * @brief 矩阵乘法: a × b
 * @param a 左矩阵
 * @param b 右矩阵
 * @return a×b
 */
export Mat4 mul(const Mat4 &a, const Mat4 &b);

/**
 * @brief 变换点 (仿射变换; 对投影矩阵结果 w≠1, 仅用于节点世界矩阵)
 * @param m 变换矩阵
 * @param p 点
 * @return 变换后的点
 */
export Vec3 transformPoint(const Mat4 &m, const Vec3 &p);

/**
 * @brief 平移矩阵
 * @param x X 位移
 * @param y Y 位移
 * @param z Z 位移
 * @return 平移矩阵
 */
export Mat4 translate(float x, float y, float z);

/**
 * @brief 绕 X 轴旋转 (弧度, 右手系)
 * @param rad 旋转角
 * @return 旋转矩阵
 */
export Mat4 rotateX(float rad);

/**
 * @brief 绕 Y 轴旋转 (弧度, 右手系)
 * @param rad 旋转角
 * @return 旋转矩阵
 */
export Mat4 rotateY(float rad);

/**
 * @brief 绕 Z 轴旋转 (弧度, 右手系)
 * @param rad 旋转角
 * @return 旋转矩阵
 */
export Mat4 rotateZ(float rad);

/**
 * @brief 缩放矩阵
 * @param sx X 缩放
 * @param sy Y 缩放
 * @param sz Z 缩放
 * @return 缩放矩阵
 */
export Mat4 scale(float sx, float sy, float sz);

/**
 * @brief 透视投影 (右手系, Vulkan 约定 NDC z∈[0,1])
 * @param fovYRad 垂直视场角 (弧度)
 * @param aspect  宽高比 width/height
 * @param zNear   近裁剪面距离
 * @param zFar    远裁剪面距离
 * @return 投影矩阵
 */
export Mat4 perspective(float fovYRad, float aspect, float zNear, float zFar);

/**
 * @brief 视图矩阵 (右手系, Y 轴向上)
 * @param eye    相机位置
 * @param center 观察目标点
 * @param up     上方向
 * @return 视图矩阵
 */
export Mat4 lookAt(const Vec3 &eye, const Vec3 &center, const Vec3 &up);

/**
 * @brief 矩阵转置: r.m[col*4+row] = m.m[row*4+col]
 * @param m 输入矩阵
 * @return 转置矩阵
 */
export Mat4 transpose(const Mat4 &m);