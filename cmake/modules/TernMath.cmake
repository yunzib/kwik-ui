# ============================================================================
# cmake/modules/TernMath.cmake — 3D 数学库 (TernMath)
#
# C++20 模块库, 提供 `TernMath` 模块 (import TernMath;)。
# 接口: modules/ternmath/ternmath.cppm
# 实现: src/ternmath/ternmath.cpp
# 替代原 kwik_matx (include/matx/matx.h + src/matx/matx.cpp) — 去除命名空间。
# ============================================================================

# 1. 创建库目标
add_library(TernMath)

# 2. 添加 C++20 模块接口文件 (PUBLIC_MODULES)
target_sources(TernMath
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_SOURCE_DIR}/modules/ternmath
        FILES
            modules/ternmath/ternmath.cppm
    PRIVATE
        src/ternmath/ternmath.cpp
)

# 3. 安装 / 导出
install(TARGETS TernMath
    EXPORT KwiKUITargets
    FILE_SET cxx_modules DESTINATION share/kwik-ui/modules/ternmath
    ARCHIVE DESTINATION lib
)