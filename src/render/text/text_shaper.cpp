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
// 禁则字符集（简表，D3 断行用；shaper 按码点打标，layout 按标回溯）
//   行首禁则：闭标点类（句读/收引号/收括号/中点/省略破折），不可居行首
//   行尾禁则：开括号类（开引号/开括号），不可居行尾
// ═══════════════════════════════════════════════════════════════════════════
static bool isKinsokuNoLineStart(uint32_t cp) {
    switch (cp) {
    case 0x3001: case 0x3002:                                                // 、。
    case 0xFF01: case 0xFF0C: case 0xFF0E: case 0xFF1A: case 0xFF1B: case 0xFF1F:    // ！，．：；？
    case 0xFF09: case 0xFF5D: case 0xFF3D:                                   // ）｝】
    case 0x3011: case 0x3015: case 0x3017:                                   // 】〕〉
    case 0x300B: case 0x300D: case 0x300F:                                   // 》」』
    case 0x00B7: case 0x30FB: case 0x2026: case 0x2014: case 0xFF5E:         // ·・…—～
    case '!': case '?': case ',': case '.': case ';': case ':': case ')': case ']': case '}':
        return true;
    default:
        return false;
    }
}
static bool isKinsokuNoLineEnd(uint32_t cp) {
    switch (cp) {
    case 0xFF08: case 0xFF3B: case 0xFF5B:                                   // （［｛
    case 0x3010: case 0x3014: case 0x3008: case 0x300A: case 0x300C: case 0x300E:    // 【〔〈《「『
    case '(': case '[': case '{':
        return true;
    default:
        return false;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// shapeText — 完整排版字形序列
// ═══════════════════════════════════════════════════════════════════════════

auto TextShaper::shapeText(FontId fontId, const char *text, float fontSize, float dpiScale)
    -> std::vector<ShapedGlyph> {
    std::vector<ShapedGlyph> result;

    auto *face = fontManager_.getFace(fontId);
    if (!face || !text || !text[0]) { return result; }

    auto *ftFace = static_cast<FreeTypeTextFace *>(face)->ftFace();
    if (!ftFace) { return result; }
    // 保存主字体指针和 ascender，回落字体字形需对齐主字体基线
    auto *primaryFtFace = ftFace;
    float primaryAscender = static_cast<float>(primaryFtFace->size->metrics.ascender) / 64.0f;

    // ── 设定 HarfBuzz 缩放 + FreeType 像素尺寸 ──
    float pixelSize = std::round(fontSize * dpiScale);
    hb_font_set_scale(face->harfbuzzFont(), static_cast<int>(pixelSize * 64.0f), static_cast<int>(pixelSize * 64.0f));
    FT_Set_Pixel_Sizes(ftFace, 0, (FT_UInt)pixelSize);

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
    // ── Helper: 码点的 UTF-8 字节长（按首字节；调用方保证 offset 落在码点首） ──
    auto decodeUtf8Len = [](const char *s, int offset) -> uint8_t {
        unsigned char c = (unsigned char)s[offset];
        if ((c & 0x80) == 0) return 1;
        if ((c & 0xE0) == 0xC0) return 2;
        if ((c & 0xF0) == 0xE0) return 3;
        return 4;
    };

    for (unsigned int i = 0; i < glyphCount; i++) {
        const uint32_t gid = glyphInfo[i].codepoint;
        const float xOff = static_cast<float>(glyphPos[i].x_offset) / 64.0f;
        const float yOff = static_cast<float>(glyphPos[i].y_offset) / 64.0f;
        const float xAdv = static_cast<float>(glyphPos[i].x_advance) / 64.0f;

        // ── \n 换行符检测 ──────────────────────────────────
        if (gid == 0 && glyphInfo[i].cluster < UINT32_MAX) {
            uint32_t cp = decodeUtf8Cp(text, (int)glyphInfo[i].cluster);
            if (cp == '\n') {
                ShapedGlyph sg;
                sg.fontId = fontId;
                sg.cluster = glyphInfo[i].cluster;
                sg.numBytes = 1;    // \n 恒 1 字节
                sg.fontSize = fontSize;    // 空行行高按标准行高算，须携带字号
                sg.isNewline = true;
                sg.advanceX = 0;
                result.push_back(sg);
                cursorX += xAdv;
                continue;
            }
        }

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
        float baselineAdjust = 0.0f;
        if (activeFont != fontId) {
            FT_Set_Pixel_Sizes(ftFace, 0, (FT_UInt)pixelSize);
            // 回落字体的基线偏移量 = 主字体 ascender - 回落字体 ascender
            float fbAscender = static_cast<float>(ftFace->size->metrics.ascender) / 64.0f;
            baselineAdjust = primaryAscender - fbAscender;
        }
        face->loadGlyph(activeGid, FT_LOAD_DEFAULT | FT_LOAD_TARGET_LIGHT);

        // float scaleToLogical = fontSize / pixelSize;
        float scaleToLogical = 1.0f / dpiScale;    // 统一到物理 1:1 网格(旧 1.0833 → 1.046)
        // D1：回退字形 advance 用回退字体真实度量——主字体 hb_shape 对缺字
        // 字形给出的是 notdef 步进，emoji/缺字字符宽度会错（过宽/过窄/重叠）
        float advPhysical = xAdv;
        if (activeFont != fontId) {
            advPhysical = static_cast<float>(ftFace->glyph->metrics.horiAdvance) / 64.0f;
        }
        ShapedGlyph sg;
        sg.fontId = activeFont;
        sg.glyphIndex = activeGid;
        sg.fontSize = fontSize;
        sg.x = (cursorX + xOff + static_cast<float>(ftFace->glyph->metrics.horiBearingX) / 64.0f) * scaleToLogical;
        sg.y = (cursorY + yOff - static_cast<float>(ftFace->glyph->metrics.horiBearingY) / 64.0f + baselineAdjust)
               * scaleToLogical;
        sg.advanceX = advPhysical * scaleToLogical;
        sg.width = static_cast<float>(ftFace->glyph->metrics.width) / 64.0f * scaleToLogical;
        sg.height = static_cast<float>(ftFace->glyph->metrics.height) / 64.0f * scaleToLogical;
        sg.cluster = glyphInfo[i].cluster;
        // 码点字节长 + 禁则标记（断行回溯/修剪的判定数据，见 text_layout）
        {
            uint32_t cp = decodeUtf8Cp(text, (int)glyphInfo[i].cluster);
            sg.numBytes = decodeUtf8Len(text, (int)glyphInfo[i].cluster);
            sg.isSpace = (cp == 0x20 || cp == 0x3000);
            sg.noLineStart = isKinsokuNoLineStart(cp);
            sg.noLineEnd = isKinsokuNoLineEnd(cp);
        }
        result.push_back(sg);
        cursorX += advPhysical;
    }

    return result;
}