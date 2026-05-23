// stb_image 实现隔离文件 — 不使用 C++20 modules
// 独立翻译单元避免 operator new 与 import std 冲突
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"