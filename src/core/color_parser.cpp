module;

#include <stdint.h>
#include <ctype.h>

module kwik.core.color_parser;

import kwik.core.types;

Color parseColor(const std::string &str) {
    std::string s = str;

    // 去除首尾空白
    s.erase(0, s.find_first_not_of(" \t\n\r"));
    s.erase(s.find_last_not_of(" \t\n\r") + 1);

    if (s.empty()) { return Color::transparent(); }

    // #RGB 格式
    if (s.size() == 4 && s[0] == '#') {
        return {static_cast<uint8_t>(parseHex(s[1]) * 17), static_cast<uint8_t>(parseHex(s[2]) * 17),
                static_cast<uint8_t>(parseHex(s[3]) * 17), 255};
    }

    // #RRGGBB 格式
    if (s.size() == 7 && s[0] == '#') {
        return {static_cast<uint8_t>((parseHex(s[1]) << 4) | parseHex(s[2])),
                static_cast<uint8_t>((parseHex(s[3]) << 4) | parseHex(s[4])),
                static_cast<uint8_t>((parseHex(s[5]) << 4) | parseHex(s[6])), 255};
    }

    // #RRGGBBAA 格式
    if (s.size() == 9 && s[0] == '#') {
        return {static_cast<uint8_t>((parseHex(s[1]) << 4) | parseHex(s[2])),
                static_cast<uint8_t>((parseHex(s[3]) << 4) | parseHex(s[4])),
                static_cast<uint8_t>((parseHex(s[5]) << 4) | parseHex(s[6])),
                static_cast<uint8_t>((parseHex(s[7]) << 4) | parseHex(s[8]))};
    }

    // 颜色名称
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    if (s == "transparent") return Color::transparent();
    if (s == "black") return Color::black();
    if (s == "white") return Color::white();
    if (s == "red") return Color::red();
    if (s == "green") return Color::green();
    if (s == "blue") return Color::blue();

    // rgba(r, g, b, a) 格式
    if (s.substr(0, 5) == "rgba(") {
        size_t pos = 5;
        int r = 0, g = 0, b = 0;
        float a = 1.0f;

        // 简化解析
        auto nextNum = [&s, &pos]() -> int {
            while (pos < s.size() && !std::isdigit(s[pos]) && s[pos] != '.') pos++;
            int start = pos;
            while (pos < s.size() && (std::isdigit(s[pos]) || s[pos] == '.')) pos++;
            return std::stoi(s.substr(start, pos - start));
        };

        r = nextNum();
        g = nextNum();
        b = nextNum();
        a = nextNum();    // rgba a 已经是 [0,1] 范围，无需除 255

        return {static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b),
                static_cast<uint8_t>(a * 255)};
    }

    // rgb(r, g, b) 格式
    if (s.substr(0, 4) == "rgb(") {
        size_t pos = 4;
        auto nextNum = [&s, &pos]() -> int {
            while (pos < s.size() && !std::isdigit(s[pos])) pos++;
            int start = pos;
            while (pos < s.size() && std::isdigit(s[pos])) pos++;
            return std::stoi(s.substr(start, pos - start));
        };

        return {static_cast<uint8_t>(nextNum()), static_cast<uint8_t>(nextNum()), static_cast<uint8_t>(nextNum()), 255};
    }

    return Color::transparent();
}
