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



####################### 三方库配置 ##########################################
message(STATUS "===== 开始引入三方库 =====")
#quickjs
add_subdirectory(third_party/quickjs)
message(STATUS "===== 完成引入 quickjs =====")

#vulkan
# find_package(Vulkan REQUIRED)
#   target_link_libraries(project_target PRIVATE Vulkan::Vulkan)


################################## 项目配置 ######################################
# ==================== 平台选项定义 ====================
# option(KWIK_USE_VULKAN "Enable Vulkan rendering backend" ON)
# option(KWIK_USE_SOFTWARE "Enable software rendering backend" ON)
# option(KWIK_USE_OPENGL "Enable OpenGL rendering backend" OFF)


# 包含模块配置
include(cmake/modules/Utils.cmake)
include(cmake/modules/Core.cmake)
include(cmake/modules/Element.cmake)
include(cmake/modules/Platform.cmake)
include(cmake/modules/Render.cmake)
include(cmake/modules/Engine.cmake)

# 构建选项
option(KWIK_BUILD_TESTS "Build tests" OFF)
option(KWIK_BUILD_EXAMPLES "Build examples" ON)

# 创建聚合库（可选）
add_library(kwik INTERFACE)
target_link_libraries(kwik INTERFACE
    kwik_utils
    kwik_core
    kwik_platform
    kwik_engine
    kwik_element
    kwik_render
    
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




