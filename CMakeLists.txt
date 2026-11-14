cmake_minimum_required(VERSION 4.3.2)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# 启用 C++ 模块支持
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
# 生成 compile_command.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# 启用 C++23 标准库模块导入
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD  "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
set(CMAKE_EXPERIMENTAL_MAPPED_PACKAGE_INFO "ababa1b5-7099-495f-a9cd-e22d38f274f2")
set(CMAKE_EXPERIMENTAL_GENERATE_SBOM "ca494ed3-b261-4205-a01f-603c95e4cae0")

# 版本语义自 v0.1.0 启动（定版锚点，回滚/分界用）
project(KwiKUI VERSION 0.1.0 LANGUAGES C CXX)

# ── 安装前缀（默认 build/install）──
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Install prefix")

# ============================================================================
# 构建类型配置
#
# 切换方式: 修改下面 KWIK_BUILD_TYPE 的值后执行 cmake -B build -G Ninja
#   "Debug"       — 完整调试符号, 无优化 (~28 MB)
#   "Release"     — O3 + LTO + gc-sections (~3-4 MB)
#   "MinSizeRel"  — Os + LTO + gc-sections (~2-3 MB)
# ============================================================================
set(KWIK_BUILD_TYPE "Debug" CACHE STRING "Debug | Release | MinSizeRel")
set_property(CACHE KWIK_BUILD_TYPE PROPERTY STRINGS Debug Release MinSizeRel)
set(CMAKE_BUILD_TYPE "${KWIK_BUILD_TYPE}" CACHE STRING "" FORCE)
# ── Debug flags ──────────────────────────────────────────
set(CMAKE_CXX_FLAGS_DEBUG   "-g -Og"       CACHE STRING "")
set(CMAKE_C_FLAGS_DEBUG     "-g -Og"       CACHE STRING "")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG FALSE)
# ── Release flags ────────────────────────────────────────
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -ffunction-sections -fdata-sections"
    CACHE STRING "")
set(CMAKE_C_FLAGS_RELEASE   "-O3 -DNDEBUG -ffunction-sections -fdata-sections"
    CACHE STRING "")
# IPO(LTO)：Clang 全档启用；GNU 16 暂关——全局 IPO 使 vendored 三方归档同为
# LTO 对象，与 COFF 成员混合时 lto1 报 "COFF object format mismatch"
# （harfbuzz 实测）。挂任务清单跟踪，根因明确后 GNU 再开
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL FALSE)
else()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
endif()
# ── GCC 运行时静态链接：保持单 exe 分发 ──
# niXman GCC 默认动态链 libstdc++-6.dll/libgcc_s_seh-1.dll/libwinpthread-1.dll；
# 仅 static-libgcc/libstdc++ 不够——libwinpthread-1.dll 仍动态，运行时 PATH
# 撞上旧版（缺 clock_gettime64 入口点，0xc0000139）。-static 将三者全内嵌
# （系统 DLL kernel32 等与 Vulkan-1 为导入库不受影响）；llvm-mingw Clang
# 本就默认静态运行时，勿加此类旗标（libc++ 无此选项，加了反告警）
add_link_options(
    "$<$<CXX_COMPILER_ID:GNU>:-static>"
    # GCC16 模块+PE：模块接口的 inline 函数局部静态（shared_ptr __tag 等）
    # 在 BMI 消费者 obj 中重复实例化为强符号 → 多定义。同一实体语义安全，
    # 先放行保 spike（挂任务清单跟踪根因）
    "$<$<CXX_COMPILER_ID:GNU>:-Wl,--allow-multiple-definition>"
)

# std::print 运行时（print.cc 的 __open_terminal 等）自 GCC15 拆入 libstdc++exp.a，
# g++ 默认不链。link_libraries 使其位于链接行对象/库区之后（GNU ld 归档按需
# 拉入依赖出现顺序——add_link_options 会排在对象之前导致永不拉入）
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    link_libraries(stdc++exp)
endif()

# ── C++26 静态反射（P2996）──
# GCC16 需显式 -freflection（-std=c++26 不自动开启）；std 模块已收编 <meta>
# （stdc++.h:238）故 import std 即得 std::meta。-freflection 按目标注入
# （Bridge.cmake 起，后续随反射面扩展），不放全局——三方目标（harfbuzz
# 自设低标准）会被全局旗标打崩。Clang 主线无实现——Clang 口径已随反射
# 引入退役（见 CHANGELOG 0.1.1）

# ── 死代码裁剪（Release/MinSizeRel 配合 -ffunction-sections）──
add_link_options(
    "$<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:Clang,GNU>>:LINKER:--gc-sections>"
)
# ── MinSizeRel flags ─────────────────────────────────────
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG -ffunction-sections -fdata-sections"
    CACHE STRING "")
set(CMAKE_C_FLAGS_MINSIZEREL   "-Os -DNDEBUG -ffunction-sections -fdata-sections"
    CACHE STRING "")
# MINSIZEREL 的 IPO 已随上方 GNU/Clang 分支统一设置
add_link_options(
    "$<$<AND:$<CONFIG:MinSizeRel>,$<CXX_COMPILER_ID:Clang,GNU>>:LINKER:--gc-sections>"
)


# ── MinGW + libc++ C++26: operator new(size, align_val_t) 歧义 ──
# llvm-mingw 的 libc++ 在分配 vector<View*> 等类型时生成的
# __builtin_operator_new(size, align_val_t) 与 MinGW CRT 提供的
# 全局 operator new 产生重载歧义，导致编译失败。
#
# 参考: https://github.com/llvm/llvm-project/issues/58908
#
# -fno-aligned-allocation 禁止编译器生成 aligned 重载调用，
# 回退到普通 operator new(size_t)，对齐由标准库内部保证。
# 仅 llvm-mingw 的 libc++ 需要此 workaround；GCC/libstdc++ 不受
# 该 issue 影响，误开反而禁用正常对齐分配路径。
if(MINGW AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_STANDARD GREATER_EQUAL 26)
    add_compile_options(-fno-aligned-allocation)
endif()

# ── Windows API 基线：显式 Win10 ──
# llvm-mingw 头默认基线较新（此前无需显式定义）；经典 mingw-w64 头（GCC）
# 默认基线偏低，需显式声明才暴露 GetDpiForWindow / WM_DPICHANGED 等 API
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00 WINVER=0x0A00)
endif()

# ── 构建工具（始终构建）──
add_subdirectory(tools)

# ==================== 三方依赖（统一入口）====================
# 快照内嵌：全部三方源码随主仓分发，克隆即编译（提取规则见 third_party/README.md）
include(cmake/modules/Dependencies.cmake)

# ── C++26 静态反射（P2996，GCC16）──
# 位置纪律：必须在三方依赖 include 之后——目录 COMPILE_OPTIONS 向后继承，
# 前置会把 -freflection 泄给三方（harfbuzz 自设低标准 → "only supported
# with -std=c++26" 编译崩）。此后创建的第一方目标（含 std 模块目标
# __cmake_cxx_std_26，generate 期挂顶层目录属性）全部携带该旗标：
# <meta> 头以 __cpp_impl_reflection（核心语言宏，仅 -freflection 定义）
# 门控（meta:40）——std 模块自身也须带旗标构建，否则 std::meta 整体缺席
# std BMI。Clang 主线无实现——Clang 口径已随反射引入退役（CHANGELOG 0.1.1）
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-freflection)
endif()


################################## 项目配置 ######################################
# ==================== 平台选项定义 ====================
# option(KWIK_USE_VULKAN "Enable Vulkan rendering backend" ON)
# option(KWIK_USE_SOFTWARE "Enable software rendering backend" ON)
# option(KWIK_USE_OPENGL "Enable OpenGL rendering backend" OFF)

include_directories(include)

# 包含模块配置
include(cmake/modules/Utils.cmake)
include(cmake/modules/Core.cmake)
include(cmake/modules/Element.cmake)
include(cmake/modules/Platform.cmake)
include(cmake/modules/Render.cmake)
include(cmake/modules/Engine.cmake)
include(cmake/modules/Bridge.cmake)
include(cmake/modules/Event.cmake)
include(cmake/modules/App.cmake)
include(cmake/modules/Bytecode.cmake)
include(cmake/modules/TernMath.cmake)
include(cmake/modules/Extension.cmake)

# 构建选项
option(KWIK_BUILD_TESTS "Build tests" OFF)
option(KWIK_BUILD_EXAMPLES "Build examples" ON)

option(KWIK_ENABLE_VALIDATION "Enable Vulkan validation layer" OFF)
if(KWIK_ENABLE_VALIDATION)
    target_compile_definitions(kwik_render PRIVATE KWIK_ENABLE_VALIDATION)
endif()

# 创建聚合库（可选）
add_library(kwik INTERFACE)
target_link_libraries(kwik INTERFACE
    kwik_utils
    kwik_core
    TernMath
    kwik_platform
    kwik_engine
    kwik_element
    kwik_render
    kwik_bridge
    kwik_event
    kwik_app
)
# 扩展库条件链接：KWIK_ENABLE_* 关闭或守卫未过时目标不存在则跳过（防 configure 报错）
foreach(_ext kwik_ext_video kwik_ext_chart kwik_ext_g3d)
    if(TARGET ${_ext})
        target_link_libraries(kwik INTERFACE ${_ext})
    endif()
endforeach()

# 示例程序
if(KWIK_BUILD_EXAMPLES)
    enable_testing()    # 须在顶层（或 add_subdirectory 之前）调用，ctest 才能发现子目录用例
    add_subdirectory(test)
endif()


# 输出配置摘要
message(STATUS "========================================")
message(STATUS "Kwik-UI 模块化构建配置")
message(STATUS "========================================")
message(STATUS "核心模块: kwik_core")
message(STATUS "元素模块: kwik_element")
message(STATUS "平台模块: kwik_platform (${KWIK_PLATFORM_BACKEND})")
message(STATUS "渲染模块: kwik_render")
message(STATUS "  - Vulkan: ${KWIK_RENDER_VULKAN}")
message(STATUS "  - 软件渲染: ${KWIK_RENDER_SOFTWARE}")
message(STATUS "  - OpenGL: ${KWIK_RENDER_OPENGL}")
message(STATUS "引擎模块: kwik_engine")
message(STATUS "聚合库: kwik")
message(STATUS "Examples: ${KWIK_BUILD_EXAMPLES}")
message(STATUS "========================================")

# ==================== 安装 / 导出 ====================
# ==================== 三方库头文件 ====================
install(FILES third_party/quickjs/quickjs.h DESTINATION include/quickjs)
install(DIRECTORY third_party/freetype/include/ DESTINATION include)
install(DIRECTORY third_party/harfbuzz/src/
    DESTINATION include/harfbuzz
    FILES_MATCHING PATTERN "*.h")
install(DIRECTORY third_party/nanosvg/src/
    DESTINATION include/nanosvg
    FILES_MATCHING PATTERN "*.h")
    
install(FILES
    "$<TARGET_FILE:qjs>"
    "$<TARGET_FILE:qjs-libc>"
    DESTINATION lib
)
# ==================== CMake Config ====================
include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/KwiKUIConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/kwik-uiConfig.cmake
    INSTALL_DESTINATION share/cmake/kwik-ui
)
install(FILES 
    ${CMAKE_CURRENT_BINARY_DIR}/kwik-uiConfig.cmake
    cmake/modules/Bytecode.cmake         
    DESTINATION share/cmake/kwik-ui
)
install(FILES
    include/kwik/app_js.h
    include/kwik/bytecode_module.h
    DESTINATION include/kwik
)

# ==================== 编译定义 ====================
# 如果存在 build 目录的 shaders 等需要保留，在此追加 install 规则


## 安装命令   cmake --install build --prefix build/install


