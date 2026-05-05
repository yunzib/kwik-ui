# cmake/modules/Element.cmake - Element模块配置（不依赖utilities工具）

# 1. 创建库目标
add_library(kwik_element)

# 2. 添加C++20模块接口文件（PUBLIC_MODULES）
target_sources(kwik_element
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules/element
        FILES
            modules/element/props.cppm
            modules/element/view.cppm
            modules/element/text.cppm

        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules/layout
        FILES
            modules/layout/flex_layout.cppm
            modules/layout/grid_layout.cppm
            modules/layout/stack_layout.cppm
            modules/layout/scroll_view.cppm
            
    PRIVATE
        src/element/view.cpp
        src/element/text.cpp

        src/layout/flex_layout.cpp
        src/layout/grid_layout.cpp
        src/layout/stack_layout.cpp
        src/layout/scroll_view.cpp
)

# 5. 添加依赖（DEPENDS）
target_link_libraries(kwik_element
    PRIVATE
        kwik_core
        kwik_engine
        kwik_render
        qjs
)

# 6. 添加编译定义（COMPILE_DEFINITIONS）
target_compile_definitions(kwik_element
    PRIVATE
        KWIK_ELEMENT_MODULE
)