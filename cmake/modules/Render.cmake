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
    modules/render/texture_manager.cppm
    modules/render/text/text_types.cppm
    modules/render/text/text_face.cppm
    modules/render/text/text_shaper.cppm
    modules/render/text/text_font_manager.cppm
    modules/render/text/text_layout_engine.cppm
    modules/render/text/text_layout_cache.cppm
    modules/render/text/text_glyph_cache.cppm
    modules/render/text/text_render_pipeline.cppm
    
)
set(RENDER_PRIVATE_SOURCES
    src/render/graphics.cpp
    src/render/command.cpp
    src/render/render_thread.cpp
    src/render/text/text_face_ft.cpp
    src/render/text/text_shaper.cpp
    src/render/text/text_font_manager.cpp
    src/render/text/text_layout_engine.cpp
    src/render/text/text_layout_cache.cpp
    src/render/text/text_glyph_cache.cpp
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
    )
    list(APPEND RENDER_PRIVATE_SOURCES
        src/render/vulkan/vulkan_context.cpp
        src/render/vulkan/vulkan_rect_renderer.cpp
        src/render/vulkan/vulkan_glyph_renderer.cpp
        src/render/vulkan/vulkan_image_renderer.cpp
        src/render/vulkan/vulkan_clip_manager.cpp
        src/render/vulkan/vulkan_backend.cpp
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



# ============================================================================
# Shader compilation — compile GLSL to SPIR-V, embed in C++ header
# ============================================================================
find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)
if(NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found — install Vulkan SDK")
endif()
set(SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_GEN_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_GEN_DIR})
add_custom_command(
    OUTPUT ${SHADER_GEN_DIR}/rect_shaders.h
    COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_SRC_DIR}/rect.vert -o ${SHADER_GEN_DIR}/rect.vert.spv
    COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_SRC_DIR}/rect.frag -o ${SHADER_GEN_DIR}/rect.frag.spv
    COMMAND ${CMAKE_COMMAND}
        -DVERT_SPV=${SHADER_GEN_DIR}/rect.vert.spv
        -DFRAG_SPV=${SHADER_GEN_DIR}/rect.frag.spv
        -DOUTPUT=${SHADER_GEN_DIR}/rect_shaders.h
        -DNAME=kRect
        -P ${SHADER_SRC_DIR}/spv_to_header.cmake
    DEPENDS ${SHADER_SRC_DIR}/rect.vert ${SHADER_SRC_DIR}/rect.frag ${SHADER_SRC_DIR}/spv_to_header.cmake
    COMMENT "Compiling shaders to embedded SPIR-V header"
)
target_include_directories(kwik_render PRIVATE ${SHADER_GEN_DIR})
target_sources(kwik_render PRIVATE ${SHADER_GEN_DIR}/rect_shaders.h)

add_custom_command(
    OUTPUT ${SHADER_GEN_DIR}/glyph_shaders.h
    COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_SRC_DIR}/glyph.vert -o ${SHADER_GEN_DIR}/glyph.vert.spv
    COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_SRC_DIR}/glyph.frag -o ${SHADER_GEN_DIR}/glyph.frag.spv
    COMMAND ${CMAKE_COMMAND}
        -DVERT_SPV=${SHADER_GEN_DIR}/glyph.vert.spv
        -DFRAG_SPV=${SHADER_GEN_DIR}/glyph.frag.spv
        -DOUTPUT=${SHADER_GEN_DIR}/glyph_shaders.h
        -DNAME=kGlyph
        -P ${SHADER_SRC_DIR}/spv_to_header.cmake
    DEPENDS ${SHADER_SRC_DIR}/glyph.vert ${SHADER_SRC_DIR}/glyph.frag ${SHADER_SRC_DIR}/spv_to_header.cmake
    COMMENT "Compiling glyph shaders to embedded SPIR-V header"
)
target_sources(kwik_render PRIVATE ${SHADER_GEN_DIR}/glyph_shaders.h)

add_custom_command(
    OUTPUT ${SHADER_GEN_DIR}/image_shaders.h
    COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_SRC_DIR}/image.vert -o ${SHADER_GEN_DIR}/image.vert.spv
    COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_SRC_DIR}/image.frag -o ${SHADER_GEN_DIR}/image.frag.spv
    COMMAND ${CMAKE_COMMAND}
        -DVERT_SPV=${SHADER_GEN_DIR}/image.vert.spv
        -DFRAG_SPV=${SHADER_GEN_DIR}/image.frag.spv
        -DOUTPUT=${SHADER_GEN_DIR}/image_shaders.h
        -DNAME=kImage
        -P ${SHADER_SRC_DIR}/spv_to_header.cmake
    DEPENDS ${SHADER_SRC_DIR}/image.vert ${SHADER_SRC_DIR}/image.frag ${SHADER_SRC_DIR}/spv_to_header.cmake
    COMMENT "Compiling image shaders to embedded SPIR-V header"
)
target_sources(kwik_render PRIVATE ${SHADER_GEN_DIR}/image_shaders.h)


install(TARGETS kwik_render
    EXPORT KwiKUITargets
    FILE_SET cxx_modules DESTINATION share/kwik-ui/modules/render
    ARCHIVE DESTINATION lib
)