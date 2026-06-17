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

project(KwiKUI LANGUAGES C CXX)

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
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
add_link_options(
    "$<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:Clang>>:LINKER:--gc-sections>"
)
# ── MinSizeRel flags ─────────────────────────────────────
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG -ffunction-sections -fdata-sections"
    CACHE STRING "")
set(CMAKE_C_FLAGS_MINSIZEREL   "-Os -DNDEBUG -ffunction-sections -fdata-sections"
    CACHE STRING "")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
add_link_options(
    "$<$<AND:$<CONFIG:MinSizeRel>,$<CXX_COMPILER_ID:Clang>>:LINKER:--gc-sections>"
)



####################### 三方库配置 ##########################################
message(STATUS "===== 开始引入三方库 =====")
#quickjs
add_subdirectory(third_party/quickjs)
message(STATUS "===== 完成引入 quickjs =====")

#vulkan
# find_package(Vulkan REQUIRED)
#   target_link_libraries(project_target PRIVATE Vulkan::Vulkan)

# FreeType — 先关闭 HarfBuzz 动态加载，关闭不必要的外部依赖
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "")
set(FT_DISABLE_ZLIB ON CACHE BOOL "")
set(FT_DISABLE_BZIP2 ON CACHE BOOL "")
set(FT_DISABLE_PNG ON CACHE BOOL "")
set(FT_DISABLE_BROTLI ON CACHE BOOL "")
add_subdirectory(third_party/freetype)
message(STATUS "===== 完成引入 freetype =====")
# HarfBuzz — 此时 freetype target 已存在，会自动开启 HB_HAVE_FREETYPE
set(HB_BUILD_UTILS OFF CACHE BOOL "")
set(HB_BUILD_SUBSET OFF CACHE BOOL "")
add_subdirectory(third_party/harfbuzz)
message(STATUS "===== 完成引入 harfbuzz =====")
target_link_libraries(freetype PRIVATE harfbuzz)
target_compile_definitions(freetype PRIVATE FT_CONFIG_OPTION_USE_HARFBUZZ)

# stb_image
include_directories(third_party/stb)
message(STATUS "===== 完成引入 stb_image =====")

# nanosvg
add_subdirectory(third_party/nanosvg)


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

# 构建选项
option(KWIK_BUILD_TESTS "Build tests" OFF)
option(KWIK_BUILD_EXAMPLES "Build examples" ON)

option(KWIK_ENABLE_VALIDATION "Enable Vulkan validation layer" ON)
if(KWIK_ENABLE_VALIDATION)
    target_compile_definitions(kwik_render PRIVATE KWIK_ENABLE_VALIDATION)
endif()

# 创建聚合库（可选）
add_library(kwik INTERFACE)
target_link_libraries(kwik INTERFACE
    kwik_utils
    kwik_core
    kwik_platform
    kwik_engine
    kwik_element
    kwik_render
    kwik_bridge
    kwik_event
    kwik_app
)

# 示例程序
if(KWIK_BUILD_EXAMPLES)
    add_subdirectory(examples)
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


##################### test #############################################




