// ═══════════════════════════════════════════════════════════════════════════
// 探针（诊断用，不入正式测试集）：TextShaper 对 \n / \r\n 的塑形实况
//
// 目的：验证 text_shaper.cpp 的 LF 硬断行标记链路——
//   ① LF 是否以字形形式出现在 hb 结果里（还是被 HB 整体吞掉）
//   ② gid 是否为 0（shaper 的 isNewline 判定门槛是 `gid == 0`）
//   ③ CRLF 的 \r 会成为什么（占位字形/跳过）
// ═══════════════════════════════════════════════════════════════════════════
import std;

import kwik.core.types;
import kwik.render.text.font.manager;
import kwik.render.text.shaper;
import kwik.render.text.types;

int main() {
    FontManager fm;
    FontId fid = fm.loadFont("resources/fonts/NotoSansSC-Regular.otf");
    if (fid == kInvalidFontId) {
        std::println("LOAD FONT FAILED");
        return 2;
    }
    TextShaper shaper(fm);

    const char *samples[] = {
        "AB\nCD",       // 纯 LF
        "AB\r\nCD",     // CRLF（Windows 原始文件实况）
        "AB\rCD",       // 孤立 CR
    };
    for (const char *t : samples) {
        auto gs = shaper.shapeText(fid, t, 16.0f, 1.0f);
        std::println("== \"{}\" -> {} glyphs", t, gs.size());
        for (auto &g : gs)
            std::println("   gid={} cluster={} bytes={} isNewline={} isSpace={} adv={:.2f}",
                         g.glyphIndex, g.cluster, g.numBytes,
                         g.isNewline, g.isSpace, g.advanceX);
    }
    return 0;
}
