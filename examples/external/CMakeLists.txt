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

project(kwik-demo LANGUAGES C CXX)

# ── 默认 Debug 构建 ──
# 命令行覆盖：cmake -B build -DCMAKE_BUILD_TYPE=Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING
        "Debug=filesystem+hotreload, Release=embedded bytecode" FORCE)
endif()

find_package(kwik-ui REQUIRED)

add_executable(kwik-demo main.cpp)
kwik_js(kwik-demo ENTRY ../../test/ui/test.js)
target_link_libraries(kwik-demo PRIVATE kwik-ui::kwik)

target_sources(kwik-demo PRIVATE
  FILE_SET cxx_modules TYPE CXX_MODULES
  BASE_DIRS "${kwik-ui_MODULES_DIR}"
  FILES ${kwik-ui_MODULE_FILES}
)




# 用法
# build.bat              # Debug（默认）→ IS_DEV_BUILD=1 → 文件系统 + 热重载
# build.bat Release      # Release 覆盖 → IS_DEV_BUILD=0 → 嵌入式字节码, 发布时使用
# # 或手动:
# cmake -B build -G Ninja -DCMAKE_PREFIX_PATH=../../build/install -DCMAKE_BUILD_TYPE=Release
# cmake --build build