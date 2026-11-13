# cmake/modules/Extension.cmake - 可选扩展元素构建配置
# （三方依赖已全部迁至 Dependencies.cmake/FFmpeg.cmake；
#   本文件只定义扩展插件库本体。FFmpeg 可用性读缓存变量 KWIK_HAVE_FFMPEG）

# ── video 扩展（KWIK_ENABLE_VIDEO / KWIK_VIDEO_FFMPEG 选项声明于 FFmpeg.cmake）──
if(KWIK_ENABLE_VIDEO)
    # 扩展静态库: 单模块 kwik.ext.video (1 接口 + 多实现单元)
    add_library(kwik_ext_video)
    target_sources(kwik_ext_video
        PUBLIC
            FILE_SET cxx_modules TYPE CXX_MODULES
            BASE_DIRS ${CMAKE_SOURCE_DIR}/extensions/video
            FILES
                extensions/video/interface/video.cppm
        PRIVATE
            extensions/video/impl/video.cpp
            extensions/video/impl/bindings.cpp
    )
    if(KWIK_HAVE_FFMPEG)
        # FFmpeg 播放器单实现单元（会话线程/时钟/视频帧出口/音频预留同处）
        target_sources(kwik_ext_video PRIVATE extensions/video/impl/ffmpeg/player.cpp)
        # GNU ld 归档单遍扫描：依赖者在前、被依赖者在后（lld-link 会重扫，
        # 旧序在 Clang 下无感；avutil 是 swscale/avcodec 的被依赖方须殿后）
        target_link_libraries(kwik_ext_video PRIVATE
            ff_avformat ff_avcodec ff_swscale ff_swresample ff_avutil)
        add_dependencies(kwik_ext_video ffmpeg_ext)
        target_compile_definitions(kwik_ext_video PRIVATE KWIK_VIDEO_FFMPEG=1)
    endif()
    target_link_libraries(kwik_ext_video
        PRIVATE
            kwik_core
            kwik_element
            kwik_render
            kwik_bridge
            kwik_engine
            kwik_utils   # kwik_snprintf（C printf 桥在 utils；GNU ld 归档单遍扫描要求定义库排在使用库之后）
            qjs
    )
    target_compile_definitions(kwik_ext_video PRIVATE KWIK_EXT_VIDEO_MODULE)
endif()

# ── 图表扩展（零第三方依赖；OFF = 不编译图表子系统 —— 框架裁剪）──
option(KWIK_ENABLE_CHART "Build chart extension" ON)
if(KWIK_ENABLE_CHART)
    add_library(kwik_ext_chart)
    target_sources(kwik_ext_chart
        PUBLIC
            FILE_SET cxx_modules TYPE CXX_MODULES
            BASE_DIRS ${CMAKE_SOURCE_DIR}/extensions/chart
            FILES
                extensions/chart/interface/chart.cppm
        PRIVATE
            extensions/chart/impl/chart.cpp
            extensions/chart/impl/chart_renderers.cpp
            extensions/chart/impl/bindings.cpp
    )
    target_link_libraries(kwik_ext_chart
        PRIVATE
            kwik_core
            kwik_element
            kwik_render
            kwik_bridge
            kwik_engine
            kwik_utils   # kwik_snprintf（C printf 桥在 utils；定义库须排在使用库之后）
            qjs
    )
    target_compile_definitions(kwik_ext_chart PRIVATE KWIK_EXT_CHART_MODULE)
endif()

# ── G3D 3D 扩展 ──
option(KWIK_ENABLE_G3D "Build G3D extension" ON)
if(KWIK_ENABLE_G3D)
    # 快照守卫（防御性）：源码随主仓分发，理论必然存在；缺则跳过防误删后 configure 报错
    if(NOT EXISTS ${CMAKE_SOURCE_DIR}/third_party/fastgltf/CMakeLists.txt)
        message(STATUS "G3D 扩展：third_party/fastgltf 缺失，跳过构建")
    else()
        add_library(kwik_ext_g3d)
        target_sources(kwik_ext_g3d
            PUBLIC
                FILE_SET cxx_modules TYPE CXX_MODULES
                BASE_DIRS ${CMAKE_SOURCE_DIR}/extensions/g3d
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
                kwik_utils   # kwik_snprintf（C printf 桥在 utils；定义库须排在使用库之后）
                qjs
                kwik_stb      # stb_image.h 声明（材质贴图解码；实现符号来自 kwik_element）
                TernMath
                fastgltf
        )
        target_include_directories(kwik_ext_g3d PRIVATE ${CMAKE_SOURCE_DIR}/include)
        target_compile_definitions(kwik_ext_g3d PRIVATE KWIK_EXT_G3D_MODULE)
    endif()
endif()