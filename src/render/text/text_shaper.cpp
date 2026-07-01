module;

#include <hb.h>
#include <hb-ft.h>

module kwik.render.text.shaper;

import std;
import kwik.core.types;
import kwik.render.text.types;
import kwik.render.text.font.manager;   // FontManager + FreeTypeTextFace

// ═══════════════════════════════════════════════════════════════════════════
// HarfBuzz 线程局部 buffer（避免重复创建）
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief 获取线程局部的 HarfBuzz buffer
 * @return hb_buffer_t*（已 reset，可直接 add_utf8）
 *
 * 每个线程首次调用时创建，后续复用并 reset，
 * 避免每次排版时重复分配/释放。
 */
static hb_buffer_t* getHbBuffer() {
    thread_local hb_buffer_t* buf = nullptr;
    if (!buf) {
        buf = hb_buffer_create();
    } else {
        hb_buffer_reset(buf);
    }
    return buf;
}

// ═══════════════════════════════════════════════════════════════════════════
// 构造
// ═══════════════════════════════════════════════════════════════════════════

TextShaper::TextShaper(FontManager& fontManager)
    : fontManager_(fontManager) {
}

// ═══════════════════════════════════════════════════════════════════════════
// shapeText — 完整排版字形序列
// ═══════════════════════════════════════════════════════════════════════════

auto TextShaper::shapeText(FontId fontId, const char* text, float fontSize) -> std::vector<ShapedGlyph> {
    std::vector<ShapedGlyph> result;

    auto* face = fontManager_.getFace(fontId);
    if (!face || !text || !text[0]) {
        return result;
    }

    auto* ftFace = static_cast<FreeTypeTextFace*>(face)->ftFace();
    if (!ftFace) {
        return result;
    }

    // ── 设定 HarfBuzz 缩放 + FreeType 像素尺寸 ──
    hb_font_set_scale(face->harfbuzzFont(),
                      static_cast<int>(fontSize * 64.0f),
                      static_cast<int>(fontSize * 64.0f));
    FT_Set_Pixel_Sizes(ftFace, 0, static_cast<FT_UInt>(fontSize));

    // ── HarfBuzz 排版 ──
    auto* buf = getHbBuffer();
    hb_buffer_add_utf8(buf, text, -1, 0, -1);
    hb_buffer_guess_segment_properties(buf);
    hb_shape(face->harfbuzzFont(), buf, nullptr, 0);

    // ── 遍历排版结果 ──
    unsigned int glyphCount = 0;
    auto* glyphInfo = hb_buffer_get_glyph_infos(buf, &glyphCount);
    auto* glyphPos  = hb_buffer_get_glyph_positions(buf, &glyphCount);

    float cursorX = 0.0f, cursorY = 0.0f;
    for (unsigned int i = 0; i < glyphCount; i++) {
        const uint32_t gid  = glyphInfo[i].codepoint;
        const float    xOff = static_cast<float>(glyphPos[i].x_offset) / 64.0f;
        const float    yOff = static_cast<float>(glyphPos[i].y_offset) / 64.0f;
        const float    xAdv = static_cast<float>(glyphPos[i].x_advance) / 64.0f;

        // 字体回退: 当前字体缺少该字形则尝试 fallback
        FontId activeFont = fontId;
        uint32_t activeGid = gid;
        if (gid == 0 && glyphInfo[i].cluster < UINT32_MAX) {
            // 从 HarfBuzz 取原始 codepoint (UTF-8 → Unicode)
            unsigned int cluster  = glyphInfo[i].cluster;
            unsigned int count    = 0;
            uint32_t     cp       = 0;
            // 提取该 cluster 的首个 Unicode 码点用于 fallback 查询
            const unsigned char *utf8 = reinterpret_cast<const unsigned char *>(text);
            // (简化: 直接通过 glyphInfo 反查 — 实际需要保存 text 的 cp 或从 buffer 提取)
            // ... 此处在完整实现中需要保存原始 codepoint 映射
        }
        if (activeGid == 0) {
            // 无字形可用，跳过该字符（避免渲染 .notdef 方框）
            cursorX += xAdv;
            continue;
        }

        face = fontManager_.getFace(activeFont);
        if (!face) { cursorX += xAdv; continue; }
        auto *ftFace = static_cast<FreeTypeTextFace *>(face)->ftFace();
        if (!ftFace) { cursorX += xAdv; continue; }

        face->loadGlyph(activeGid);

        ShapedGlyph sg;
        sg.fontId     = activeFont;
        sg.glyphIndex = activeGid;
        sg.fontSize   = fontSize;
        sg.x          = cursorX + xOff + static_cast<float>(ftFace->glyph->bitmap_left);
        sg.y          = cursorY + yOff - static_cast<float>(ftFace->glyph->bitmap_top);
        sg.advanceX   = xAdv;
        sg.width      = static_cast<float>(ftFace->glyph->metrics.width)  / 64.0f;
        sg.height     = static_cast<float>(ftFace->glyph->metrics.height) / 64.0f;
        sg.bearingX   = static_cast<float>(ftFace->glyph->bitmap_left);
        sg.bearingY   = static_cast<float>(ftFace->glyph->bitmap_top);
        result.push_back(sg);
        cursorX += xAdv;
    }

    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// shapeMetrics — 纯排版度量（无字形图像信息）
// ═══════════════════════════════════════════════════════════════════════════

auto TextShaper::shapeMetrics(FontId fontId, const char* text, float fontSize) -> std::vector<GlyphMetrics> {
    std::vector<GlyphMetrics> result;

    auto* face = fontManager_.getFace(fontId);
    if (!face || !text || !text[0]) {
        return result;
    }

    auto* ftFace = static_cast<FreeTypeTextFace*>(face)->ftFace();
    if (!ftFace) {
        return result;
    }

    hb_font_set_scale(face->harfbuzzFont(),
                      static_cast<int>(fontSize * 64.0f),
                      static_cast<int>(fontSize * 64.0f));
    FT_Set_Pixel_Sizes(ftFace, 0, static_cast<FT_UInt>(fontSize));

    auto* buf = getHbBuffer();
    hb_buffer_add_utf8(buf, text, -1, 0, -1);
    hb_buffer_guess_segment_properties(buf);
    hb_shape(face->harfbuzzFont(), buf, nullptr, 0);

    unsigned int glyphCount = 0;
    auto* glyphInfo = hb_buffer_get_glyph_infos(buf, &glyphCount);
    auto* glyphPos  = hb_buffer_get_glyph_positions(buf, &glyphCount);

    float cursorX = 0.0f;
    for (unsigned int i = 0; i < glyphCount; i++) {
        const uint32_t gid  = glyphInfo[i].codepoint;
        const float    xOff = static_cast<float>(glyphPos[i].x_offset) / 64.0f;
        const float    xAdv = static_cast<float>(glyphPos[i].x_advance) / 64.0f;

        face->loadGlyph(gid);

        GlyphMetrics m;
        m.glyphIndex = gid;
        m.x          = cursorX + xOff;
        m.advanceX   = xAdv;
        m.bearingX   = static_cast<float>(ftFace->glyph->bitmap_left);
        m.bearingY   = static_cast<float>(ftFace->glyph->bitmap_top);

        result.push_back(m);
        cursorX += xAdv;
    }

    return result;
}