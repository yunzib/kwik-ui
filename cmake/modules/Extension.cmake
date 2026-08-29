# cmake/modules/Extension.cmake - 可选扩展元素构建配置

# 视频扩展开关 (ON=编译 video 插件, OFF=不编译)
option(KWIK_ENABLE_VIDEO "Build video extension" ON)

if(KWIK_ENABLE_VIDEO)
    # 扩展静态库: 单模块 kwik.ext.video (1 接口 + 2 实现单元)
    add_library(kwik_ext_video)
    target_sources(kwik_ext_video
        PUBLIC
            FILE_SET cxx_modules TYPE CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/extensions/video
            FILES
                extensions/video/interface/video.cppm
        PRIVATE
            extensions/video/impl/video.cpp
            extensions/video/impl/bindings.cpp
    )
    target_link_libraries(kwik_ext_video
        PRIVATE
            kwik_core
            kwik_element
            kwik_render
            kwik_bridge
            kwik_engine
            qjs
    )
    target_compile_definitions(kwik_ext_video PRIVATE KWIK_EXT_VIDEO_MODULE)
endif()

# G3D 3D 扩展开关
option(KWIK_ENABLE_G3D "Build G3D extension" ON)

if(KWIK_ENABLE_G3D)
    # fastgltf — glTF 模型加载 (仅 G3D 需要, 从根 CMakeLists 迁入)
    set(FASTGLTF_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(FASTGLTF_ENABLE_CPP_MODULES OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/fastgltf)

    add_library(kwik_ext_g3d)
    target_sources(kwik_ext_g3d
        PUBLIC
            FILE_SET cxx_modules TYPE CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/extensions/g3d
            FILES
                extensions/g3d/interface/g3d.cppm
        PRIVATE
            extensions/g3d/impl/g3d.cpp
            extensions/g3d/impl/g3d_gltf.cpp
            extensions/g3d/impl/bindings.cpp
    )
    target_link_libraries(kwik_ext_g3d
        PRIVATE
            kwik_core
            kwik_element
            kwik_render
            kwik_bridge
            kwik_engine
            qjs
            TernMath
            fastgltf
    )
    target_include_directories(kwik_ext_g3d PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_definitions(kwik_ext_g3d PRIVATE KWIK_EXT_G3D_MODULE)
endif()