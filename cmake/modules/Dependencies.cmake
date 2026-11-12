# cmake/modules/Dependencies.cmake — 三方依赖统一引入入口
#
# 纪律：
#   1) 全部三方库为快照内嵌（源码随主仓 git 分发），目录缺失即报错
#   2) 新增三方依赖必须在本文件登记，禁止在其它文件 add_subdirectory(third_party/*)
#   3) 快照源码已含 patches/<lib>/ 全部补丁；升级走 scripts/vendor_update.ps1
#   4) FFmpeg 为 configure+make 型特殊构建，构建驱动独立于 FFmpeg.cmake，
#      由本文件统一 include（入口唯一，管理一致）

# ── 快照库守卫：目录缺失即报错（防静默残缺）──────────────────
foreach(_lib IN ITEMS quickjs freetype harfbuzz nanosvg simdjson FFmpeg fastgltf msdfgen)
    if(NOT EXISTS ${CMAKE_SOURCE_DIR}/third_party/${_lib})
        message(FATAL_ERROR
            "third_party/${_lib} 缺失：快照库随主仓克隆分发，请检查克隆完整性"
            "（git status / 重新 git clone）")
    endif()
endforeach()

# ── quickjs：JS 引擎（target: qjs / qjs-libc，被 App/Bridge/Engine 引用）──
set(QJS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/quickjs)

# ── freetype：字体光栅化（裁剪全部可选外部依赖，harfbuzz 由下方接线）──
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "")
set(FT_DISABLE_ZLIB     ON CACHE BOOL "")
set(FT_DISABLE_BZIP2    ON CACHE BOOL "")
set(FT_DISABLE_PNG      ON CACHE BOOL "")
set(FT_DISABLE_BROTLI   ON CACHE BOOL "")
add_subdirectory(third_party/freetype)

# ── harfbuzz：文本整形（关闭 utils/subset 附加产物）────────────
set(HB_BUILD_UTILS OFF CACHE BOOL "")
set(HB_BUILD_SUBSET OFF CACHE BOOL "")
add_subdirectory(third_party/harfbuzz)

# ── freetype ↔ harfbuzz 接线（原根 CMakeLists.txt:116-120 原样迁入）──
target_link_libraries(freetype PRIVATE harfbuzz)
target_compile_definitions(freetype PRIVATE
    FT_CONFIG_OPTION_USE_HARFBUZZ
    FT_CONFIG_OPTION_SUBPIXEL_RENDERING
)

# ── stb：头文件库（仅 stb_image.h），INTERFACE 目标取代全局 include_directories ──
add_library(kwik_stb INTERFACE)
target_include_directories(kwik_stb INTERFACE ${CMAKE_SOURCE_DIR}/third_party/stb)

# ── simdjson：fastgltf 的 JSON 后端。
#    必须在 add_subdirectory(fastgltf) 之前定义——
#    fastgltf/cmake/dependencies.cmake:4 检测到 TARGET 存在即跳过下载，
set(SIMDJSON_TARGET_VERSION "4.6.2")
add_library(simdjson STATIC third_party/simdjson/simdjson.cpp)
target_include_directories(simdjson PUBLIC ${CMAKE_SOURCE_DIR}/third_party/simdjson)
add_library(simdjson::simdjson ALIAS simdjson)

# ── nanosvg：SVG 解码（targets: nanosvg / nanosvgrast，由快照内上游 CMake 定义）──
add_subdirectory(third_party/nanosvg)

# ── fastgltf：glTF 加载（G3D 扩展专用；simdjson target 已就绪 → 零联网）──
set(FASTGLTF_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/fastgltf)

# ── FFmpeg：configure+make 型构建（驱动逻辑独立成文件，入口仍在本文件）──
include(${CMAKE_CURRENT_LIST_DIR}/FFmpeg.cmake)