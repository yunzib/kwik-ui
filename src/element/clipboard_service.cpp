// ============================================================================
// clipboard_service.cpp — 富文本 run 序列剪贴板格式（kwik/text-runs-v2）编解码
//
// 序列化独立成自由函数而非内联在 RuntimeClipboardService：纯逻辑可被
// core_tests 直测（读写端布局一致性/畸形输入防线），运行时侧只做字节搬运。
// ============================================================================
module;
#include <cstring>
#include <string>
#include <vector>

module kwik.element.clipboard_service;

import std;
import kwik.core.types;     // Color
import kwik.core.props;     // TextRun / TextStyle / FontWeight / FontStyle

namespace {
// 布局常量与 magic（与模块接口注释同步维护）
constexpr const char *kMagic = "KWTVR2";    // 6 字节版本魔数
constexpr size_t kHeaderSize = 6 + 4;       // magic + runCount

void appendU32(std::string &b, uint32_t v) {
    char tmp[4];
    std::memcpy(tmp, &v, 4);
    b.append(tmp, 4);
}
bool readU32(const std::string &b, size_t &off, size_t end, uint32_t &v) {
    if (off + 4 > end) return false;
    std::memcpy(&v, b.data() + off, 4);
    off += 4;
    return true;
}
}    // namespace

std::string serializeTextRuns(const std::vector<TextRun> &runs) {
    std::string b = kMagic;
    appendU32(b, static_cast<uint32_t>(runs.size()));
    for (auto &r : runs) {
        // record 本体字节数：textLen + text + fontSize + 8B 样式 + hasLink
        // +（有链接时）linkLen + link
        bool hasLink = !r.link.empty();
        uint32_t recordLen = 4 + static_cast<uint32_t>(r.text.size()) + 4 + 8 + 1;
        if (hasLink) recordLen += 4 + static_cast<uint32_t>(r.link.size());

        appendU32(b, recordLen);
        appendU32(b, static_cast<uint32_t>(r.text.size()));
        b.append(r.text);
        char tmp[4];
        std::memcpy(tmp, &r.style.fontSize, 4);
        b.append(tmp, 4);
        b.push_back(static_cast<char>(static_cast<uint8_t>(r.style.fontWeight)));
        b.push_back(static_cast<char>(static_cast<uint8_t>(r.style.fontStyle)));
        b.push_back(static_cast<char>(r.style.textColor.r));
        b.push_back(static_cast<char>(r.style.textColor.g));
        b.push_back(static_cast<char>(r.style.textColor.b));
        b.push_back(static_cast<char>(r.style.textColor.a));
        b.push_back(r.style.underline ? '\1' : '\0');
        b.push_back(r.style.strikethrough ? '\1' : '\0');
        b.push_back(hasLink ? '\1' : '\0');
        if (hasLink) {
            appendU32(b, static_cast<uint32_t>(r.link.size()));
            b.append(r.link);
        }
    }
    return b;
}

bool deserializeTextRuns(const std::string &b, std::vector<TextRun> &out) {
    if (b.size() < kHeaderSize || b.compare(0, 6, kMagic) != 0) return false;
    size_t off = 6;
    uint32_t count = 0;
    if (!readU32(b, off, b.size(), count) || count > 100000) return false;
    out.clear();
    for (uint32_t i = 0; i < count; ++i) {
        // recordLen 定界：字段越界/截断即拒（readU32 按 record 终点校验），
        // 已知字段读完后 off 直接跳到 record 终点——未知尾部字段安全忽略
        uint32_t recordLen = 0;
        if (!readU32(b, off, b.size(), recordLen) || off + recordLen > b.size()) return false;
        size_t recEnd = off + recordLen;

        uint32_t n = 0;
        if (!readU32(b, off, recEnd, n)) return false;
        TextRun r;
        if (off + n > recEnd) return false;
        r.text = b.substr(off, n);
        off += n;

        if (off + 4 > recEnd) return false;
        std::memcpy(&r.style.fontSize, b.data() + off, 4);
        off += 4;

        if (off + 8 > recEnd) return false;    // weight/italic/RGBA/underline/strikethrough
        uint8_t weight = 0, italic = 0, cr = 0, cg = 0, cb = 0, ca = 255, ul = 0, st = 0;
        std::memcpy(&weight, b.data() + off, 1);
        std::memcpy(&italic, b.data() + off + 1, 1);
        std::memcpy(&cr, b.data() + off + 2, 1);
        std::memcpy(&cg, b.data() + off + 3, 1);
        std::memcpy(&cb, b.data() + off + 4, 1);
        std::memcpy(&ca, b.data() + off + 5, 1);
        std::memcpy(&ul, b.data() + off + 6, 1);
        std::memcpy(&st, b.data() + off + 7, 1);
        off += 8;
        r.style.fontWeight = static_cast<FontWeight>(weight);
        r.style.fontStyle = static_cast<FontStyle>(italic);
        r.style.textColor = Color{cr, cg, cb, ca};
        r.style.underline = ul != 0;
        r.style.strikethrough = st != 0;

        uint8_t hasLink = 0;
        if (off + 1 > recEnd) return false;
        std::memcpy(&hasLink, b.data() + off, 1);
        ++off;
        if (hasLink) {
            uint32_t linkLen = 0;
            if (!readU32(b, off, recEnd, linkLen) || off + linkLen > recEnd) return false;
            r.link = b.substr(off, linkLen);
            off += linkLen;
        }

        off = recEnd;    // 跳过未知尾部字段（版本演进预留）
        out.push_back(std::move(r));
    }
    return !out.empty();
}
