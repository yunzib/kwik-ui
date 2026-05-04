# cmake/modules/Render.cmake - Render模块配置（不依赖utilities工具）

# 渲染后端选项
option(KWIK_RENDER_VULKAN "Enable Vulkan rendering backend" ON)
option(KWIK_RENDER_SOFTWARE "Enable software rendering backend" ON)

# 动态构建模块文件列表
set(RENDER_PUBLIC_MODULES
    modules/render/graphics.cppm
    modules/render/backend.cppm
    modules/render/command.cppm
    modules/render/render_thread.cppm
)
set(RENDER_PRIVATE_SOURCES
    src/render/graphics.cpp
    src/render/command.cpp
    src/render/render_thread.cpp
)

set(RENDER_COMPILE_DEFINITIONS
    KWIK_RENDER_MODULE
)

# 条件添加Vulkan后端
if(KWIK_RENDER_VULKAN)
    list(APPEND RENDER_PUBLIC_MODULES modules/render/vulkan_backend.cppm)
    list(APPEND RENDER_PRIVATE_SOURCES src/render/vulkan_backend.cpp)
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
