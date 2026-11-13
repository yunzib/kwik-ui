/** @file c_bridge.h
 *  @brief 经典编译 TU 提供的 C 函数桥——供模块 TU 调用
 *
 * GCC16 的 std 模块不导出 C 库函数的 std:: using 声明（using 不能从
 * 模块导出全局模块实体），且文本包含 stdio.h 会与 std 模块内同名声明
 * 产生语言链接冲突——模块 TU 因此无法直接使用 snprintf/vsnprintf。
 * 本头声明的桥函数在经典编译的 TU（src/utils/c_printf_bridge.cpp）中
 * 实现，经 extern "C" 符号解耦，模块 TU 文本包含本头即可安全调用。
 */
#ifndef KWIK_C_BRIDGE_H
#define KWIK_C_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief snprintf 桥：语义与 C snprintf 一致（含运行时格式串场景）
 *  @param buf  输出缓冲
 *  @param size 缓冲容量
 *  @param fmt  printf 格式串（可为运行时变量）
 *  @return 与 snprintf 相同（拟写入长度；截断返回值视实现，同 C 语义）
 */
int kwik_snprintf(char *buf, unsigned long long size, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // KWIK_C_BRIDGE_H
