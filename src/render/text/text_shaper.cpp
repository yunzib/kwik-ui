module;

#include <hb.h>
#include <hb-ft.h>

module kwik.render.text.shaper;

import std;
import kwik.core.types;
import kwik.render.text.types;
import kwik.render.text.font.manager; // FontManager + FreeTypeTextFace

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
static hb_buffer_t *getHbBuffer() {
    thread_local hb_buffer_t *buf = nullptr;
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

TextShaper::TextShaper(FontManager &fontManager) : fontManager_(fontManager) {}

// ═══════════════════════════════════════════════════════════════════════════
// shapeText — 完整排版字形序列
// ═══════════════════════════════════════════════════════════════════════════

auto TextShaper::shapeText(FontId fontId, const char *text, float fontSize) -> std::vector<ShapedGlyph> {
    std::vector<ShapedGlyph> result;

    auto *face = fontManager_.getFace(fontId);
    if (!face || !text || !text[0]) { return result; }

    auto *ftFace = static_cast<FreeTypeTextFace *>(face)->ftFace();
    if (!ftFace) { return result; }

    // ── 设定 HarfBuzz 缩放 + FreeType 像素尺寸 ──
    hb_font_set_scale(face->harfbuzzFont(), static_cast<int>(fontSize * 64.0f), static_cast<int>(fontSize * 64.0f));
    FT_Set_Pixel_Sizes(ftFace, 0, (FT_UInt)std::round(fontSize));

    // ── HarfBuzz 排版 ──
    auto *buf = getHbBuffer();
    hb_buffer_add_utf8(buf, text, -1, 0, -1);
    hb_buffer_guess_segment_properties(buf);
    hb_shape(face->harfbuzzFont(), buf, nullptr, 0);

    // ── 遍历排版结果 ──
    unsigned int glyphCount = 0;
    auto *glyphInfo = hb_buffer_get_glyph_infos(buf, &glyphCount);
    auto *glyphPos = hb_buffer_get_glyph_positions(buf, &glyphCount);

    float cursorX = 0.0f, cursorY = 0.0f;
    // ── Helper: 从 UTF-8 提取单个 Unicode 码点 ──
    auto decodeUtf8Cp = [](const char *s, int offset) -> uint32_t {
        const unsigned char *p = (const unsigned char *)s + offset;
        if ((*p & 0x80) == 0) return *p;
        if ((*p & 0xE0) == 0xC0) return ((*p & 0x1Fu) << 6) | (p[1] & 0x3Fu);
        if ((*p & 0xF0) == 0xE0) return ((*p & 0x0Fu) << 12) | ((p[1] & 0x3Fu) << 6) | (p[2] & 0x3Fu);
        return ((*p & 0x07u) << 18) | ((p[1] & 0x3Fu) << 12) | ((p[2] & 0x3Fu) << 6) | (p[3] & 0x3Fu);
    };

    for (unsigned int i = 0; i < glyphCount; i++) {
        const uint32_t gid = glyphInfo[i].codepoint;
        const float xOff = static_cast<float>(glyphPos[i].x_offset) / 64.0f;
        const float yOff = static_cast<float>(glyphPos[i].y_offset) / 64.0f;
        const float xAdv = static_cast<float>(glyphPos[i].x_advance) / 64.0f;

        FontId activeFont = fontId;
        uint32_t activeGid = gid;

        // 字体回退: 主字体缺少该字形 → 查询 fallback 字体
        if (gid == 0 && glyphInfo[i].cluster < UINT32_MAX) {
            uint32_t cp = decodeUtf8Cp(text, (int)glyphInfo[i].cluster);
            FontId fbFont = fontManager_.resolveForCodepoint(fontId, cp);
            if (fbFont != fontId && fbFont != kInvalidFontId) {
                auto *fbFace = fontManager_.getFace(fbFont);
                if (fbFace) {
                    auto *fbFt = static_cast<FreeTypeTextFace *>(fbFace)->ftFace();
                    if (fbFt) {
                        FT_UInt fbGid = FT_Get_Char_Index(fbFt, cp);
                        if (fbGid != 0) {
                            activeFont = fbFont;
                            activeGid = fbGid;
                        }
                    }
                }
            }
        }

        if (activeGid == 0) {
            cursorX += xAdv;
            continue;
        }

        face = fontManager_.getFace(activeFont);
        if (!face) {
            cursorX += xAdv;
            continue;
        }
        auto *ftFace = static_cast<FreeTypeTextFace *>(face)->ftFace();
        if (!ftFace) {
            cursorX += xAdv;
            continue;
        }

        // 用回退字体时需重新设定像素尺寸
        if (activeFont != fontId) { FT_Set_Pixel_Sizes(ftFace, 0, (FT_UInt)std::round(fontSize)); }
        face->loadGlyph(activeGid);

        ShapedGlyph sg;
        sg.fontId = activeFont;
        sg.glyphIndex = activeGid;
        sg.fontSize = fontSize;
        sg.x = cursorX + xOff + static_cast<float>(ftFace->glyph->bitmap_left);
        sg.y = cursorY + yOff - static_cast<float>(ftFace->glyph->bitmap_top);
        sg.advanceX = xAdv;
        sg.width = static_cast<float>(ftFace->glyph->metrics.width) / 64.0f;
        sg.height = static_cast<float>(ftFace->glyph->metrics.height) / 64.0f;
        sg.bearingX = static_cast<float>(ftFace->glyph->bitmap_left);
        sg.bearingY = static_cast<float>(ftFace->glyph->bitmap_top);
        result.push_back(sg);
        cursorX += xAdv;
    }

    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// shapeMetrics — 纯排版度量（无字形图像信息）
// ═══════════════════════════════════════════════════════════════════════════

auto TextShaper::shapeMetrics(FontId fontId, const char *text, float fontSize) -> std::vector<GlyphMetrics> {
    std::vector<GlyphMetrics> result;

    auto *face = fontManager_.getFace(fontId);
    if (!face || !text || !text[0]) { return result; }

    auto *ftFace = static_cast<FreeTypeTextFace *>(face)->ftFace();
    if (!ftFace) { return result; }

    hb_font_set_scale(face->harfbuzzFont(), static_cast<int>(fontSize * 64.0f), static_cast<int>(fontSize * 64.0f));
    FT_Set_Pixel_Sizes(ftFace, 0, (FT_UInt)std::round(fontSize));

    auto *buf = getHbBuffer();
    hb_buffer_add_utf8(buf, text, -1, 0, -1);
    hb_buffer_guess_segment_properties(buf);
    hb_shape(face->harfbuzzFont(), buf, nullptr, 0);

    unsigned int glyphCount = 0;
    auto *glyphInfo = hb_buffer_get_glyph_infos(buf, &glyphCount);
    auto *glyphPos = hb_buffer_get_glyph_positions(buf, &glyphCount);

    float cursorX = 0.0f;
    for (unsigned int i = 0; i < glyphCount; i++) {
        const uint32_t gid = glyphInfo[i].codepoint;
        const float xOff = static_cast<float>(glyphPos[i].x_offset) / 64.0f;
        const float xAdv = static_cast<float>(glyphPos[i].x_advance) / 64.0f;

        face->loadGlyph(gid);

        GlyphMetrics m;
        m.glyphIndex = gid;
        m.x = cursorX + xOff;
        m.advanceX = xAdv;
        m.bearingX = static_cast<float>(ftFace->glyph->bitmap_left);
        m.bearingY = static_cast<float>(ftFace->glyph->bitmap_top);

        result.push_back(m);
        cursorX += xAdv;
    }

    return result;
}