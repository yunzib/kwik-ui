/**
 * @file bytecode_module.h
 * @brief 公共 BytecodeModule 类型
 *
 * 由 compile_js_bundle 工具生成的代码和库的模块共同引用此类型，
 * 避免类型定义在生成文件和库之间重复。
 */
#ifndef KWIK_BYTECODE_MODULE_H
#define KWIK_BYTECODE_MODULE_H

#include <cstdint>
#include <string_view>

/** @brief 字节码模块描述 */
struct BytecodeModule {
    std::string_view name;    /**< 模块名，如 "app.js" */
    const uint8_t *data;      /**< bytecode 二进制数据 */
    int size;                 /**< bytecode 数据大小 */
};

#endif /* KWIK_BYTECODE_MODULE_H */