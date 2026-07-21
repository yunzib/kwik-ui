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

