# cmake/modules/Render.cmake - Render模块配置（不依赖utilities工具）

# 渲染后端选项
option(KWIK_RENDER_VULKAN "Enable Vulkan rendering backend" ON)
option(KWIK_RENDER_SOFTWARE "Enable software rendering backend" OFF)

# 动态构建模块文件列表
set(RENDER_PUBLIC_MODULES
    modules/render/graphics.cppm
    modules/render/backend.cppm
    modules/render/command.cppm
    modules/render/render_thread.cppm
    modules/render/command_queue.cppm
    modules/render/layer.cppm               
    modules/render/draw_list.cppm         
    modules/render/layer_tree_builder.cppm  
    modules/render/scene_builder.cppm       
    modules/render/texture_manager.cppm
    modules/render/text/text_types.cppm
    modules/render/text/text_face.cppm
    modules/render/text/text_shaper.cppm
    modules/render/text/text_font_manager.cppm
    modules/render/text/text_layout.cppm
    modules/render/text/text_cache.cppm
    modules/render/text/text_render_pipeline.cppm

    
)
set(RENDER_PRIVATE_SOURCES
    src/render/graphics.cpp
    src/render/render_thread.cpp
    src/render/command_queue.cpp
    src/render/layer.cpp                    
    src/render/draw_list.cpp              
    src/render/layer_tree_builder.cpp       
    src/render/text/text_face_ft.cpp
    src/render/text/text_shaper.cpp
    src/render/text/text_font_manager.cpp
    src/render/text/text_layout.cpp
    src/render/text/text_cache.cpp
    src/render/text/text_render_pipeline.cpp
   
)

set(RENDER_COMPILE_DEFINITIONS
    KWIK_RENDER_MODULE
)

# 条件添加Vulkan后端
if(KWIK_RENDER_VULKAN)
    list(APPEND RENDER_PUBLIC_MODULES
        modules/render/vulkan/vulkan_context.cppm
        modules/render/vulkan/vulkan_rect_renderer.cppm
        modules/render/vulkan/vulkan_glyph_renderer.cppm
        modules/render/vulkan/vulkan_image_renderer.cppm
        modules/render/vulkan/vulkan_clip_manager.cppm
        modules/render/vulkan/vulkan_backend.cppm
        modules/render/vulkan/vulkan_triangle_renderer.cppm
    )
    list(APPEND RENDER_PRIVATE_SOURCES
        src/render/vulkan/vulkan_context.cpp
        src/render/vulkan/vulkan_rect_renderer.cpp
        src/render/vulkan/vulkan_glyph_renderer.cpp
        src/render/vulkan/vulkan_image_renderer.cpp
        src/render/vulkan/vulkan_clip_manager.cpp
        src/render/vulkan/vulkan_backend.cpp
        src/render/vulkan/vulkan_triangle_renderer.cpp
    )

    list(APPEND RENDER_LINK_LIBRARIES Vulkan::Vulkan)
    list(APPEND RENDER_COMPILE_DEFINITIONS KWIK_RENDER_VULKAN)
    
    # 查找Vulkan
    find_package(Vulkan REQUIRED)
endif()

# 条件添加软件后端
if(KWIK_RENDER_SOFTWARE)
    list(APPEND RENDER_PUBLIC_MODULES modules/render/software_backend.cppm)
    list(APPEND RENDER_PRIVATE_SOURCES src/render/software_backend.cpp)
    list(APPEND RENDER_COMPILE_DEFINITIONS KWIK_RENDER_SOFTWARE)
endif()

# 1. 创建库目标
add_library(kwik_render)

# 2. 添加C++20模块接口文件（PUBLIC_MODULES）
target_sources(kwik_render
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules/render
        FILES ${RENDER_PUBLIC_MODULES}
    # 3. 添加私有实现源文件（PRIVATE_SOURCES）
    PRIVATE
        ${RENDER_PRIVATE_SOURCES}
)

# 5. 添加依赖（DEPENDS: kwik_core, kwik_platform）
target_link_libraries(kwik_render
    PRIVATE
        kwik_core
        kwik_platform
        freetype
        harfbuzz
        # msdfgen::msdfgen        # 链接 msdfgen-core + msdfgen-ext
)

# 6. 添加链接库（LINK_LIBRARIES）
if(RENDER_LINK_LIBRARIES)
    target_link_libraries(kwik_render
        PRIVATE
            ${RENDER_LINK_LIBRARIES}
    )
endif()

# 7. 添加编译定义（COMPILE_DEFINITIONS）
if(RENDER_COMPILE_DEFINITIONS)
    target_compile_definitions(kwik_render
        PRIVATE
            ${RENDER_COMPILE_DEFINITIONS}
    )
endif()

# ── Slang 着色器编译（提取到独立模块） ────────────────────────
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/Shaders.cmake)