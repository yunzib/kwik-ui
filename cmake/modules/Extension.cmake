# cmake/modules/Extension.cmake - 可选扩展元素构建配置

# 视频扩展开关 (ON=编译 video 插件, OFF=不编译)
option(KWIK_ENABLE_VIDEO "Build video extension" ON)
# FFmpeg 解码后端 (ON=构建 third_party/ffmpeg 并编译播放器；OFF=仅 Null 播放器)
option(KWIK_VIDEO_FFMPEG "Build FFmpeg media player backend" ON)

if(KWIK_ENABLE_VIDEO)
    # ── FFmpeg 源内树构建：全部步骤由 CMake 驱动，随 cmake --build 一次编译 ──
    # FFmpeg 上游只有 configure（shell 脚本）+ Makefile，无 CMake 后端：构建期
    # 由 CMake 自动调用 Git bash 跑 configure、自动修正路径风格、再调
    # mingw32-make 编静态库（首次约 30 秒，产物缓存，之后增量跳过）。
    # 工具链 llvm-mingw clang 与主工程同源；干净 PATH 防 anaconda 等 DLL 干扰
    # msys fork（实测 0xC000026B）。全链路已实测（2026-11-08）。
    set(FFMPEG_SRC ${CMAKE_SOURCE_DIR}/third_party/ffmpeg)
    set(FFMPEG_BUILD ${CMAKE_BINARY_DIR}/ffmpeg-build)
    set(KWIK_HAVE_FFMPEG OFF)
    if(KWIK_VIDEO_FFMPEG AND EXISTS ${FFMPEG_SRC}/configure)
        find_program(GIT_BASH bash.exe PATHS "C:/Program Files/Git/usr/bin" "C:/Program Files (x86)/Git/usr/bin")
        get_filename_component(CC_BIN ${CMAKE_C_COMPILER} DIRECTORY)    # llvm-mingw/bin
        find_program(FFMPEG_MAKE mingw32-make HINTS ${CC_BIN})
        if(GIT_BASH AND FFMPEG_MAKE)
            set(KWIK_HAVE_FFMPEG ON)
            get_filename_component(GIT_USR_BIN ${GIT_BASH} DIRECTORY)
            file(MAKE_DIRECTORY ${FFMPEG_BUILD})    # 构建目录 = 导入目标头文件根之一，须配置期即存在
            set(FFMPEG_ENV ${CMAKE_COMMAND} -E env "PATH=${GIT_USR_BIN}\;${CC_BIN}\;C:/Windows/System32\;C:/Windows")

            # 源码树 MSYS 路径（configure 以此记录 SRC_PATH）：C:/ws → /c/ws
            string(SUBSTRING "${FFMPEG_SRC}" 0 1 FF_DRIVE)
            string(TOLOWER "${FF_DRIVE}" FF_DRIVE)
            string(SUBSTRING "${FFMPEG_SRC}" 2 -1 FF_TAIL)
            set(FFMPEG_SRC_MSYS "/${FF_DRIVE}${FF_TAIL}")
            # 路径风格修正脚本（配置期生成，无外置文件）：configure 写入的 /c/... 换
            # C:/...——mingw32-make 按 Windows API 解析，MSYS 路径会报 No rule to make target
            file(WRITE ${CMAKE_BINARY_DIR}/ffmpeg_fixpaths.cmake
"foreach(f Makefile ffbuild/config.mak)
    file(READ \"\${DIR}/\${f}\" c)
    string(REPLACE \"${FFMPEG_SRC_MSYS}\" \"${FFMPEG_SRC}\" c \"\${c}\")
    file(WRITE \"\${DIR}/\${f}\" \"\${c}\")
endforeach()
")
            # 最小 LGPL 组件集（批 4 本地播放；批 5 网络在此追加协议/TLS）
            set(FFMPEG_FLAGS
                --cc=clang --ar=llvm-ar --nm=llvm-nm --ranlib=llvm-ranlib
                --disable-everything --disable-programs --disable-doc --disable-avdevice
                --disable-avfilter --disable-network --disable-autodetect
                --disable-gpl --disable-x86asm --enable-static --disable-shared
                --enable-demuxer=mov,matroska
                --enable-decoder=h264,hevc,vp8,vp9,mpeg4
                --enable-parser=h264,hevc,vp8,vp9,mpeg4video
                --enable-protocol=file
                --enable-swresample --enable-swscale)
            set(FFMPEG_LIBS
                ${FFMPEG_BUILD}/libavformat/libavformat.a
                ${FFMPEG_BUILD}/libavcodec/libavcodec.a
                ${FFMPEG_BUILD}/libswscale/libswscale.a
                ${FFMPEG_BUILD}/libswresample/libswresample.a
                ${FFMPEG_BUILD}/libavutil/libavutil.a)
            # ① configure + 路径修正（config.mak 存在即跳过）
            add_custom_command(
                OUTPUT ${FFMPEG_BUILD}/ffbuild/config.mak
                COMMAND ${FFMPEG_ENV} ${GIT_BASH} ${FFMPEG_SRC_MSYS}/configure ${FFMPEG_FLAGS}
                COMMAND ${CMAKE_COMMAND} -DDIR=${FFMPEG_BUILD} -P ${CMAKE_BINARY_DIR}/ffmpeg_fixpaths.cmake
                WORKING_DIRECTORY ${FFMPEG_BUILD}
                COMMENT "Configuring FFmpeg (minimal LGPL set, llvm-mingw)"
                VERBATIM)
            # ② 编静态库（直出构建目录，不跑 make install）
            add_custom_command(
                OUTPUT ${FFMPEG_LIBS}
                COMMAND ${FFMPEG_ENV} ${FFMPEG_MAKE} -j8
                        libavutil/libavutil.a libswresample/libswresample.a libswscale/libswscale.a
                        libavcodec/libavcodec.a libavformat/libavformat.a
                DEPENDS ${FFMPEG_BUILD}/ffbuild/config.mak
                WORKING_DIRECTORY ${FFMPEG_BUILD}
                COMMENT "Building FFmpeg static libraries"
                VERBATIM)
            add_custom_target(ffmpeg_ext DEPENDS ${FFMPEG_LIBS})
            # 导入目标：头文件双根——源码树（libav*/*.h）+ 构建目录（config.h / 各库 version.h）
            foreach(ff_lib avformat avcodec swscale swresample avutil)
                add_library(ff_${ff_lib} STATIC IMPORTED GLOBAL)
                set_target_properties(ff_${ff_lib} PROPERTIES
                    IMPORTED_LOCATION ${FFMPEG_BUILD}/lib${ff_lib}/lib${ff_lib}.a
                    INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_BUILD};${FFMPEG_SRC}"
                    INTERFACE_LINK_LIBRARIES "ws2_32;bcrypt;ole32")    # Windows 静态链接常规系统依赖
                add_dependencies(ff_${ff_lib} ffmpeg_ext)
            endforeach()
        else()
            message(STATUS "FFmpeg 后端：未找到 Git bash / mingw32-make，降级为 Null 播放器")
        endif()
    endif()

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
        # FFmpeg 播放器单实现单元（会话线程/时钟/视频帧出口/音频预留同处，内部类型不外泄）
        target_sources(kwik_ext_video PRIVATE extensions/video/impl/ffmpeg/player.cpp)
        target_link_libraries(kwik_ext_video PRIVATE ff_avcodec ff_avformat ff_avutil ff_swscale ff_swresample)
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