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