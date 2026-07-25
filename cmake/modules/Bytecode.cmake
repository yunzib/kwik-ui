# ============================================================================
# CMake 函数: kwik_js
#
# 用途:
#   自动将应用 JS 文件编译为字节码并注册到目标可执行文件。
#   开发（Debug）构建中 IS_DEV_BUILD=1，应用程序从文件系统加载 JS；
#   生产（Release）构建中 IS_DEV_BUILD=0，应用程序从嵌入式字节码加载。
#
# 参数:
#   TARGET  —— 目标可执行文件名称（必填，第一个位置参数）
#   ENTRY   —— 入口 JS 文件路径（可选，默认 app.js）
#              可以是相对路径（相对于 ${CMAKE_CURRENT_SOURCE_DIR}）
#              也可以是绝对路径
#
# 用法示例:
#   kwik_js(my_app)                          # 默认入口 app.js
#   kwik_js(my_app ENTRY src/main.js)         # 自定义入口和子目录
#   kwik_js(my_app ENTRY ../shared/app.js)    # 相对父目录
#
# 工作原理:
#   ① add_custom_command 注册构建规则，当任意 .js 文件变更时
#      自动重新运行 compile_js_bundle 工具。
#   ② compile_js_bundle 工具编译入口 JS 及其所有 import 子模块，
#      生成 js_bytecode.h（字节码数据）和 kwik_js_reg.cpp（注册代码）。
#   ③ kwik_js_reg.cpp 通过静态初始化调用 kwik_register_app_js()
#      将字节码模块表注入 Application 运行时。
#   ④ 自动注入 IS_DEV_BUILD 预处理器宏供 Application::init() 使用。
#
# 依赖:
#   compile_js_bundle 工具必须随库安装并在 KwiKUIConfig.cmake 中
#   注册为 kwik-ui::compile_js_bundle IMPORTED 目标。
# ============================================================================

function(kwik_js TARGET)
    # ── 解析可选参数 ──
    #     cmake_parse_arguments 解析函数参数：
    #       PARSE_ARGV 1 表示从第一个位置参数之后开始解析
    #       "" 表示无布尔标志参数
    #       "ENTRY" 表示接受一个字符串参数 ENTRY
    #       "" 表示无列表参数
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "ENTRY" "")

    # ── 确定入口文件路径 ──
    #     未指定 ENTRY 时默认为 ${CMAKE_CURRENT_SOURCE_DIR}/app.js
    if(NOT ARG_ENTRY)
        set(ARG_ENTRY "app.js")
    endif()
    #     将相对路径转为绝对路径
    if(NOT IS_ABSOLUTE "${ARG_ENTRY}")
        set(ARG_ENTRY "${CMAKE_CURRENT_SOURCE_DIR}/${ARG_ENTRY}")
    endif()

    # ── 搜集所有 JS 源文件作为自定义命令的依赖 ──
    #     从入口文件所在目录向下递归搜集，这样任一 JS 文件变更
    #     都会自动触发 compile_js_bundle 重新编译
    get_filename_component(ENTRY_DIR "${ARG_ENTRY}" DIRECTORY)
    file(GLOB_RECURSE JS_FILES "${ENTRY_DIR}/*.js")

    # ── 生成目录（在构建目录下） ──
    set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/kwik_js")

    # ── 注册自定义构建命令 ──
    #     COMMAND 通过 kwik-ui::compile_js_bundle IMPORTED 目标引用
    #     工具路径，CMake 在构建时自动解析到安装目录下的二进制。
    #     依赖包含 compile_js_bundle 工具本身和所有 JS 源文件。
    add_custom_command(
        OUTPUT
            "${GEN_DIR}/js_bytecode.h"       # 字节码数据（头文件）
            "${GEN_DIR}/kwik_js_reg.cpp"      # 静态注册代码（源文件）
        COMMAND
            kwik-ui::compile_js_bundle       # IMPORTED 目标 → 自动解析路径
            "${ARG_ENTRY}"                    # 入口 JS 文件
            -o "${GEN_DIR}/js_bytecode.h"     # 输出：字节码数据头文件
            --c-output "${GEN_DIR}/kwik_js_reg.cpp"  # 输出：注册源文件
        DEPENDS
            kwik-ui::compile_js_bundle        # 工具变更时也要重跑
            ${JS_FILES}                       # 任一 JS 变更时重跑
        COMMENT
            "[KwiK] 编译 JS → bytecode..."
    )

    # ── 将注册源文件编译进目标 ──
    #     kwik_js_reg.cpp 使用 #if !IS_DEV_BUILD 条件编译，
    #     Debug 构建时不产生实际注册代码。
    target_sources("${TARGET}" PRIVATE "${GEN_DIR}/kwik_js_reg.cpp")
    target_include_directories("${TARGET}" PRIVATE "${GEN_DIR}")

    # ── 注入构建类型宏 ──
    #     $<CONFIG:Debug> 生成器表达式：
    #       Debug   配置 → IS_DEV_BUILD=1
    #       非 Debug 配置 → IS_DEV_BUILD=0
    #     Application::init() 根据此宏在运行时选择加载路径
    target_compile_definitions("${TARGET}" PRIVATE
        "IS_DEV_BUILD=$<IF:$<CONFIG:Debug>,1,0>")
endfunction()