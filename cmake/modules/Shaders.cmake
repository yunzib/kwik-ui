# cmake/modules/Shaders.cmake — Slang 着色器编译配置
#
# 由 cmake/modules/Render.cmake 通过 include() 引入。
# 依赖 kwik_render 目标已定义。

# ── 查找 slangc ───────────────────────────────────────────────
find_program(SLANGC slangc REQUIRED)
if(NOT SLANGC)
    message(FATAL_ERROR "slangc not found — install Vulkan SDK with Slang")
endif()

# ── 目录 ──────────────────────────────────────────────────────
set(SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_GEN_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_GEN_DIR})

# ── Rect ──────────────────────────────────────────────────────
add_custom_command(
    OUTPUT ${SHADER_GEN_DIR}/rect_shaders.h
    COMMAND ${SLANGC} ${SHADER_SRC_DIR}/rect.slang
            -entry vertexMain -stage vertex
            -target spirv -profile glsl_450
            -o ${SHADER_GEN_DIR}/rect.vert.spv
    COMMAND ${SLANGC} ${SHADER_SRC_DIR}/rect.slang
            -entry fragmentMain -stage fragment
            -target spirv -profile glsl_450
            -o ${SHADER_GEN_DIR}/rect.frag.spv
    COMMAND ${CMAKE_COMMAND}
        -DVERT_SPV=${SHADER_GEN_DIR}/rect.vert.spv
        -DFRAG_SPV=${SHADER_GEN_DIR}/rect.frag.spv
        -DOUTPUT=${SHADER_GEN_DIR}/rect_shaders.h
        -DNAME=kRect
        -P ${SHADER_SRC_DIR}/spv_to_header.cmake
    DEPENDS ${SHADER_SRC_DIR}/rect.slang ${SHADER_SRC_DIR}/spv_to_header.cmake
    COMMENT "Compiling rect shaders to embedded SPIR-V header"
)
target_include_directories(kwik_render PRIVATE ${SHADER_GEN_DIR})
target_sources(kwik_render PRIVATE ${SHADER_GEN_DIR}/rect_shaders.h)

# ── Glyph ─────────────────────────────────────────────────────
add_custom_command(
    OUTPUT ${SHADER_GEN_DIR}/glyph_shaders.h
    COMMAND ${SLANGC} ${SHADER_SRC_DIR}/glyph.slang
            -entry vertexMain -stage vertex
            -target spirv -profile glsl_450
            -o ${SHADER_GEN_DIR}/glyph.vert.spv
    COMMAND ${SLANGC} ${SHADER_SRC_DIR}/glyph.slang
            -entry fragmentMain -stage fragment
            -target spirv -profile glsl_450
            -o ${SHADER_GEN_DIR}/glyph.frag.spv
    COMMAND ${CMAKE_COMMAND}
        -DVERT_SPV=${SHADER_GEN_DIR}/glyph.vert.spv
        -DFRAG_SPV=${SHADER_GEN_DIR}/glyph.frag.spv
        -DOUTPUT=${SHADER_GEN_DIR}/glyph_shaders.h
        -DNAME=kGlyph
        -P ${SHADER_SRC_DIR}/spv_to_header.cmake
    DEPENDS ${SHADER_SRC_DIR}/glyph.slang ${SHADER_SRC_DIR}/spv_to_header.cmake
    COMMENT "Compiling glyph shaders to embedded SPIR-V header"
)
target_sources(kwik_render PRIVATE ${SHADER_GEN_DIR}/glyph_shaders.h)

# ── Image ─────────────────────────────────────────────────────
add_custom_command(
    OUTPUT ${SHADER_GEN_DIR}/image_shaders.h
    COMMAND ${SLANGC} ${SHADER_SRC_DIR}/image.slang
            -entry vertexMain -stage vertex
            -target spirv -profile glsl_450
            -o ${SHADER_GEN_DIR}/image.vert.spv
    COMMAND ${SLANGC} ${SHADER_SRC_DIR}/image.slang
            -entry fragmentMain -stage fragment
            -target spirv -profile glsl_450
            -o ${SHADER_GEN_DIR}/image.frag.spv
    COMMAND ${CMAKE_COMMAND}
        -DVERT_SPV=${SHADER_GEN_DIR}/image.vert.spv
        -DFRAG_SPV=${SHADER_GEN_DIR}/image.frag.spv
        -DOUTPUT=${SHADER_GEN_DIR}/image_shaders.h
        -DNAME=kImage
        -P ${SHADER_SRC_DIR}/spv_to_header.cmake
    DEPENDS ${SHADER_SRC_DIR}/image.slang ${SHADER_SRC_DIR}/spv_to_header.cmake
    COMMENT "Compiling image shaders to embedded SPIR-V header"
)
target_sources(kwik_render PRIVATE ${SHADER_GEN_DIR}/image_shaders.h)

# ── Triangle ──────────────────────────────────────────────────
add_custom_command(
    OUTPUT ${SHADER_GEN_DIR}/triangle_shaders.h
    COMMAND ${SLANGC} ${SHADER_SRC_DIR}/triangle.slang
            -entry vertexMain -stage vertex
            -target spirv -profile glsl_450
            -o ${SHADER_GEN_DIR}/triangle.vert.spv
    COMMAND ${SLANGC} ${SHADER_SRC_DIR}/triangle.slang
            -entry fragmentMain -stage fragment
            -target spirv -profile glsl_450
            -o ${SHADER_GEN_DIR}/triangle.frag.spv
    COMMAND ${CMAKE_COMMAND}
        -DVERT_SPV=${SHADER_GEN_DIR}/triangle.vert.spv
        -DFRAG_SPV=${SHADER_GEN_DIR}/triangle.frag.spv
        -DOUTPUT=${SHADER_GEN_DIR}/triangle_shaders.h
        -DNAME=kTriangle
        -P ${SHADER_SRC_DIR}/spv_to_header.cmake
    DEPENDS ${SHADER_SRC_DIR}/triangle.slang ${SHADER_SRC_DIR}/spv_to_header.cmake
    COMMENT "Compiling triangle shaders to embedded SPIR-V header"
)
target_sources(kwik_render PRIVATE ${SHADER_GEN_DIR}/triangle_shaders.h)

# ── Mesh (3D, G3D 组件) ───────────────────────────────────────
add_custom_command(
    OUTPUT ${SHADER_GEN_DIR}/mesh_shaders.h
    COMMAND ${SLANGC} ${SHADER_SRC_DIR}/mesh.slang
            -entry vertexMain -stage vertex
            -target spirv -profile glsl_450
            -o ${SHADER_GEN_DIR}/mesh.vert.spv
    COMMAND ${SLANGC} ${SHADER_SRC_DIR}/mesh.slang
            -entry fragmentMain -stage fragment
            -target spirv -profile glsl_450
            -o ${SHADER_GEN_DIR}/mesh.frag.spv
    COMMAND ${CMAKE_COMMAND}
        -DVERT_SPV=${SHADER_GEN_DIR}/mesh.vert.spv
        -DFRAG_SPV=${SHADER_GEN_DIR}/mesh.frag.spv
        -DOUTPUT=${SHADER_GEN_DIR}/mesh_shaders.h
        -DNAME=kMesh
        -P ${SHADER_SRC_DIR}/spv_to_header.cmake
    DEPENDS ${SHADER_SRC_DIR}/mesh.slang ${SHADER_SRC_DIR}/spv_to_header.cmake
    COMMENT "Compiling mesh shaders to embedded SPIR-V header"
)
target_sources(kwik_render PRIVATE ${SHADER_GEN_DIR}/mesh_shaders.h)