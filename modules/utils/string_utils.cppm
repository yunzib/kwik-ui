module;

#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

export module kwik.utils.string_utils;

#if defined(_WIN32)
/**
 * @brief 将UTF-8字符串转换为宽字符字符串（Windows平台专用）
 * @param utf8 输入的UTF-8字符串
 * @return 转换后的宽字符字符串
 */
export std::wstring Utf8ToWide(const std::string &utf8);
#endif
