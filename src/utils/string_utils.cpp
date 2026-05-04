module;
#include <string>
#include <vector>
#include <windows.h>

module kwik.utils.string_utils;

std::wstring Utf8ToWide(const std::string &utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    std::vector<wchar_t> buffer(len + 1);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), buffer.data(), len);
    buffer[len] = 0;
    return buffer.data(); // 隐式转换为 std::wstring
}
