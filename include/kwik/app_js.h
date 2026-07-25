/**
 * @file app_js.h
 * @brief 字节码注册接口
 *
 * 由自动生成的 kwik_js_reg.cpp 在静态初始化时调用，
 * 将字节码模块表注入 Application。
 * 声明为 extern "C" 避免 C++ name mangling。
 */
#ifndef KWIK_APP_JS_H
#define KWIK_APP_JS_H

#include "kwik/bytecode_module.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册应用 JS 的字节码模块表
 * @param modules  BytecodeModule 数组（首元素为入口模块）
 * @param count    模块总数
 */
void kwik_register_app_js(const BytecodeModule *modules, int count);

#ifdef __cplusplus
}
#endif

#endif /* KWIK_APP_JS_H */