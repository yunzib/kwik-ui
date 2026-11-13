// c_printf_bridge.cpp — C printf 族桥实现（经典编译 TU，无任何模块 import）
// 背景见 include/kwik/c_bridge.h 头注释：模块 TU 无法文本包含 stdio.h
// （与 std 模块语言链接冲突），printf 族经本桥以 extern "C" 符号提供。
// 必须包含声明头：保证定义侧与调用侧同为 extern "C" 链接——否则定义被
// C++ 修饰（_Z13kwik_snprintf...），调用侧裸 C 符号永远解析不到
#include "kwik/c_bridge.h"
#include <cstdio>
#include <cstdarg>

int kwik_snprintf(char *buf, unsigned long long size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = std::vsnprintf(buf, static_cast<size_t>(size), fmt, ap);
    va_end(ap);
    return r;
}
