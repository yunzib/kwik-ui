
# 1. 创建库目标
add_library(kwik_utils)

# 2. 添加C++20模块接口文件（PUBLIC_MODULES）
target_sources(kwik_utils
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules/utils
        FILES
            modules/utils/utils.cppm
            modules/utils/string_utils.cppm
    
    # 添加私有实现源文件（PRIVATE_SOURCES）
    PRIVATE
        src/utils/string_utils.cpp
)


# # 5. 添加依赖（DEPENDS: kwik_core, kwik_element）
# target_link_libraries(kwik_utils
#     PRIVATE
#         kwik_core
#         kwik_element
# )

# # 6. 添加链接库（LINK_LIBRARIES: quickjs）
# target_link_libraries(kwik_utils
#     PRIVATE
#         quickjs
# )

# 7. 添加编译定义（COMPILE_DEFINITIONS）
target_compile_definitions(kwik_utils
    PRIVATE
        KWIK_UTILS_MODULE
)
