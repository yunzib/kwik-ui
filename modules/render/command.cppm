module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <atomic>

export module kwik.render.command;

import kwik.core.types;

import std;

export enum class BlendMode { SrcOver, SrcCopy };

export struct ClearCmd { Color color; };
export struct FillRectCmd {
    Rect rect;
    Color color;
    BlendMode mode = BlendMode::SrcOver;
};
export struct FillRoundedRectCmd { Rect rect; float radius; Color color; };
/** @brief 线段胶囊描边命令（折线每段一个胶囊：线段 + 两端 round cap） */
export struct DrawSegmentCmd {
    float ax, ay;   // 线段端点 A（物理坐标）
    float bx, by;   // 线段端点 B（物理坐标）
    float halfW;    // 胶囊半径 = strokeWidth / 2（物理像素）
    Color color;
};
export struct StrokeRoundedRectCmd { Rect rect; float radius; Color color; float strokeWidth; };
export struct DrawShadowCmd { Rect rect; float radius; Shadow shadow; };
export struct DrawImageCmd {
    uint32_t textureId;
    Rect rect;
    float opacity;
    float cornerRadius;
};
export struct DrawGlyphCmd {
    FontId fontId;
    uint32_t glyphIndex;
    float x, y, width, height;
    float uvLeft, uvTop, uvRight, uvBottom;
    Color color;
    float pageIndex = 0;
};
export struct FillTrianglesCmd {
    size_t vertexOffset;
    uint32_t vertexCount;
    Color color;
    BlendMode mode = BlendMode::SrcOver;
};
export struct StrokeTrianglesCmd {
    size_t vertexOffset;
    uint32_t vertexCount;
    Color color;
};


/*
 * ── 状态命令全部移除 ──
 * SaveStateCmd, RestoreStateCmd, ResetClipCmd  → 由 Layer 树承载
 * TranslateCmd, ScaleCmd, SetOpacityCmd        → 由 Layer 树承载
 * BeginFrameCmd, EndFrameCmd, PresentCmd       → 由 RenderThread 直接管理
 */

 // ── 3D 网格 (G3D 组件) ──

/**
 * @brief 3D 顶点 — 位置 + 法线 (均对象空间)
 *
 * 与 2D 的 Vec2 顶点分开存储 (meshVertices_ 独立缓冲),
 * 避免污染 2D 三角形路径的顶点格式。
 */
export struct Vertex3D {
    float x = 0.0f, y = 0.0f, z = 0.0f;      // 位置 (对象空间)
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;   // 法线 (对象空间)
};

/**
 * @brief 3D 网格绘制命令
 *
 * mvp 为列主序 4×4 (与 matx::Mat4::toArray 输出一致),
 * 以原始数组跨模块边界传递, 避免模块接口导出传统库类型。
 * lightDir 为对象空间方向光 (归一化), 由 G3D 组件 CPU 端预计算。
 */
export struct DrawMeshCmd {
    size_t vertexOffset;                     // meshVertices_ 起始顶点索引
    uint32_t vertexCount;                    // 顶点数 (三角形列表, 3 的倍数)
    float mvp[16];                           // 模型-视图-投影矩阵 (列主序)
    Color color;                             // 基础颜色
    float lightDir[3];                       // 方向光方向 (对象空间, 归一化)
    Rect viewport;                           // 元素屏幕矩形 (mesh 视口, 定位+裁剪)
};

