
# 1. 创建库目标
add_library(kwik_core)


# 2. 添加C++20模块接口文件（PUBLIC_MODULES）
target_sources(kwik_core
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_SOURCE_DIR}/modules/core
        FILES
            modules/core/types.cppm
            modules/core/constraints.cppm
            modules/core/log.cppm
    PRIVATE
        src/core/log.cpp
)

# 4. 添加编译定义（COMPILE_DEFINITIONS）
target_compile_definitions(kwik_core PRIVATE KWIK_CORE_MODULE)

