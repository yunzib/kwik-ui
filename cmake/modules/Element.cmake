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
            modules/element/typed_prop.cppm
            modules/element/view.cppm
            modules/element/rootview.cppm
            modules/element/text.cppm
            modules/element/button.cppm
            modules/element/image.cppm
            # modules/element/input.cppm
            # modules/element/radiobutton.cppm
            # modules/element/checkbox.cppm
            # modules/element/textarea.cppm
            # modules/element/dropdown.cppm
            # modules/element/slider.cppm
            # modules/element/progressbar.cppm
            # modules/element/switch.cppm
            # modules/element/line.cppm
            # modules/element/spinner.cppm
            # modules/element/table.cppm
            # modules/element/textview.cppm

        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules/layout
        FILES
            modules/layout/flex_layout.cppm
            modules/layout/grid_layout.cppm
            modules/layout/stack_layout.cppm
            # modules/layout/list_layout.cppm
            # modules/layout/radio_group.cppm
           
            
    PRIVATE
        src/element/view.cpp
        src/element/rootview.cpp
        src/element/text.cpp
        src/element/button.cpp
        src/element/image.cpp
        src/element/stb_image_impl.cpp
        src/element/svg_decoder.cpp
        # src/element/input.cpp
        # src/element/radiobutton.cpp
        # src/element/checkbox.cpp
        # src/element/textarea.cpp
        # src/element/dropdown.cpp
        # src/element/slider.cpp
        # src/element/progressbar.cpp
        # src/element/switch.cpp
        # src/element/line.cpp
        # src/element/spinner.cpp
        # src/element/table.cpp
        # src/element/textview.cpp

        src/layout/flex_layout.cpp
        src/layout/grid_layout.cpp
        src/layout/stack_layout.cpp
        # src/layout/list_layout.cpp
        # src/layout/radio_group.cpp
)

# 5. 添加依赖（DEPENDS）
target_link_libraries(kwik_element
    PRIVATE
        kwik_core
        kwik_engine
        kwik_render
        qjs
        nanosvg
        nanosvgrast
)

target_include_directories(kwik_element
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nanosvg/src
)

# 6. 添加编译定义（COMPILE_DEFINITIONS）
target_compile_definitions(kwik_element
    PRIVATE
        KWIK_ELEMENT_MODULE
)

install(TARGETS kwik_element
    ARCHIVE DESTINATION lib
    FILE_SET cxx_modules DESTINATION share/kwik-ui/modules/element
)