module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <atomic>

export module kwik.render.command;

import kwik.core.types;

import std;

export enum class BlendMode { SrcOver, SrcCopy };

export struct ClearCmd {
    Color color;
};
export struct FillRectCmd {
    Rect rect;
    Color color;
    BlendMode mode = BlendMode::SrcOver;
    Transform2D t;  
};
export struct FillRoundedRectCmd {
    Rect rect;
    float radius;
    Color color;
    Gradient gradient;   // 渐变（type==None → 普通填充，此时仅 color 生效）
    Transform2D t;  
};
/** @brief 线段胶囊描边命令（折线每段一个胶囊：线段 + 两端 round cap） */
export struct DrawSegmentCmd {
    float ax, ay;    // 线段端点 A（物理坐标）
    float bx, by;    // 线段端点 B（物理坐标）
    float halfW;     // 胶囊半径 = strokeWidth / 2（物理像素）
    Color color;
    Transform2D t;  
};
/** @brief 圆角矩形裁剪入栈（坐标已烘焙为物理像素） */
export struct PushClipCmd {
    Rect rect;
    float radius;
    Transform2D t;  
    Rect clipRect;   // 物理 AABB（scissor 用）
};

/** @brief 裁剪出栈 */
export struct PopClipCmd {};
export struct StrokeRoundedRectCmd {
    Rect rect;
    float radius;
    Color color;
    float strokeWidth;
    Transform2D t;  
};
export struct DrawShadowCmd {
    Rect rect;
    float radius;
    Shadow shadow;
    Transform2D t;  
};
export struct DrawImageCmd {
    uint32_t textureId;
    Rect rect;
    float opacity;
    float cornerRadius;
    Transform2D t;  
};
export struct DrawGlyphCmd {
    FontId fontId;
    uint32_t glyphIndex;
    float x, y, width, height;
    float uvLeft, uvTop, uvRight, uvBottom;
    Color color;
    float pageIndex = 0;
    Transform2D t;  
};
export struct FillTrianglesCmd {
    size_t vertexOffset;
    uint32_t vertexCount;
    Color color;
    BlendMode mode = BlendMode::SrcOver;
    Transform2D t;  
};
export struct StrokeTrianglesCmd {
    size_t vertexOffset;
    uint32_t vertexCount;
    Color color;
    Transform2D t;  
};

/**
 * @brief Sweep 扫描渐变参数（环形进度用，模拟主流框架 SweepGradient）
 *
 * cx/cy 为圆心（本地逻辑坐标），随矩阵一起作用于顶点，
 * 因此 rotate/scale/clip 对渐变与纯色填充一致（与 Gradient 语义相同）。
 * 起点色复用命令中的 color0。
 */
export struct SweepGrad {
    float cx = 0, cy = 0;   // 圆心（本地逻辑坐标）
    float a0 = 0, a1 = 0;   // 渐变起止角（弧度）
    Color color1;           // 渐变终点色
};

/**
 * @brief 渐变弧带命令（单条连续弧 + Sweep 角度渐变）
 *
 * 与 strokePath 的区别：附加圆心/起止角/终点色，渲染时顶点按角度插值颜色，
 * 实现"单命令 + 无缝 + GPU 平滑渐变"（替代 N 段短弧拼接）。
 */
export struct StrokeArcCmd {
    size_t vertexOffset;
    uint32_t vertexCount;
    Color  color0;          // 渐变起点色（透明度已烘焙）
    float  cx, cy;          // 圆心（本地逻辑坐标）
    float  a0, a1;          // 渐变起止角（弧度）
    Color  color1;          // 渐变终点色（透明度已烘焙）
    Transform2D t;
};

/**
 * @brief SDF 圆环命令（UberSDF 同款：后端内部生成 6 顶点 quad，零折线细分）
 *
 * 圆度/渐变/端帽全部由 fragment SDF 计算：每环仅 6 顶点 1 draw call，
 * 彻底规避折线描边在近共线圆环上的 miter join 退化（h≈0.001px → 半透明接缝）。
 * 参数均为本地逻辑坐标（随矩阵变换，与 Gradient 语义一致）。
 */
export struct FillRingCmd {
    float cx = 0, cy = 0;      // 圆心（本地逻辑坐标）
    float midR = 0;            // 圆环中径（本地逻辑坐标）
    float halfW = 0;           // 半带宽（本地逻辑坐标 = strokeWidth/2）
    float a0 = 0, a1 = 0;      // 弧起止角（弧度，span = a1 - a0）
    bool  roundCap = true;     // true=圆头端帽  false=平头软边
    float pad = 2.0f;          // quad 外扩（本地坐标，覆盖 AA 过渡带）
    Color color0;              // 弧起点色 / 纯色（透明度已烘焙）
    Color color1;              // 弧终点色（透明度已烘焙；== color0 时即纯色）
    Transform2D t;
};

/*
 * ── 状态命令 ──
 * PushClipCmd / PopClipCmd 由 Graphics 直接 append 到 CommandBuffer，
 * 渲染线程 replay 时 dispatch 到 backend 的 pushClipRoundedRect / popState。
 * transform / opacity 已由 Graphics 烘焙（坐标/颜色），不产生命令。
 */

// ── 3D 网格 (G3D 组件) ──

/**
 * @brief 3D 顶点 — 位置 + 法线 (均对象空间)
 *
 * 与 2D 的 Vec2 顶点分开存储 (meshVertices_ 独立缓冲),
 * 避免污染 2D 三角形路径的顶点格式。
 */
export struct Vertex3D {
    float x = 0.0f, y = 0.0f, z = 0.0f;       // 位置 (对象空间)
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;    // 法线 (对象空间)
};

/**
 * @brief 3D 网格绘制命令
 *
 * mvp 为列主序 4×4 (与 matx::Mat4::toArray 输出一致),
 * 以原始数组跨模块边界传递, 避免模块接口导出传统库类型。
 * lightDir 为对象空间方向光 (归一化), 由 G3D 组件 CPU 端预计算。
 */
export struct DrawMeshCmd {
    size_t vertexOffset;     // meshVertices_ 起始顶点索引
    uint32_t vertexCount;    // 顶点数 (三角形列表, 3 的倍数)
    float mvp[16];           // 模型-视图-投影矩阵 (列主序)
    Color color;             // 基础颜色
    float lightDir[3];       // 方向光方向 (对象空间, 归一化)
    Rect viewport;           // 元素屏幕矩形 (mesh 视口, 定位+裁剪)
};
