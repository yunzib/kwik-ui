# cmake/modules/FFmpeg.cmake — FFmpeg 快照库的 configure+make 构建
#
# FFmpeg 上游无 CMake 后端：本文件在构建期驱动 Git bash 运行 configure、
# 修正路径风格、mingw32-make 编静态库，全部随 cmake --build 一次完成，
# 产物缓存于 build/ffmpeg-build（增量构建跳过）。
# 工具链要求：Git bash（configure 需 sh）+ mingw32-make（llvm-mingw/bin 自带）。
# 任一工具缺失 → KWIK_HAVE_FFMPEG=OFF，video 扩展自动降级 Null 播放器。

# ── 选项声明（video 相关开关统一在此，Extension.cmake 只消费）──
option(KWIK_ENABLE_VIDEO "Build video extension" ON)             # OFF=不编 video 插件
option(KWIK_VIDEO_FFMPEG "Build FFmpeg media player backend" ON) # OFF=仅 Null 播放器

set(FFMPEG_SRC   ${CMAKE_SOURCE_DIR}/third_party/FFmpeg)
set(FFMPEG_BUILD ${CMAKE_BINARY_DIR}/ffmpeg-build)
set(KWIK_HAVE_FFMPEG OFF)

if(KWIK_ENABLE_VIDEO AND KWIK_VIDEO_FFMPEG AND EXISTS ${FFMPEG_SRC}/configure)
    find_program(GIT_BASH bash.exe
        PATHS "C:/Program Files/Git/usr/bin" "C:/Program Files (x86)/Git/usr/bin")
    get_filename_component(CC_BIN ${CMAKE_C_COMPILER} DIRECTORY)   # llvm-mingw/bin
    find_program(FFMPEG_MAKE mingw32-make HINTS ${CC_BIN})
    if(GIT_BASH AND FFMPEG_MAKE)
        set(KWIK_HAVE_FFMPEG ON)
        get_filename_component(GIT_USR_BIN ${GIT_BASH} DIRECTORY)
        file(MAKE_DIRECTORY ${FFMPEG_BUILD})  # 构建目录=导入目标头文件根，配置期即须存在
        # 干净 PATH：防 anaconda 等 DLL 干扰 msys fork（实测 0xC000026B）
        set(FFMPEG_ENV ${CMAKE_COMMAND} -E env
            "PATH=${GIT_USR_BIN}\;${CC_BIN}\;C:/Windows/System32\;C:/Windows")

        # 源码树 MSYS 路径（configure 以此记录 SRC_PATH）：C:/ws → /c/ws
        string(SUBSTRING "${FFMPEG_SRC}" 0 1 FF_DRIVE)
        string(TOLOWER "${FF_DRIVE}" FF_DRIVE)
        string(SUBSTRING "${FFMPEG_SRC}" 2 -1 FF_TAIL)
        set(FFMPEG_SRC_MSYS "/${FF_DRIVE}${FF_TAIL}")
        # 路径风格修正脚本（配置期生成）：configure 写入的 /c/... 换回 C:/...——
        # mingw32-make 按 Windows API 解析，MSYS 路径会报 No rule to make target
        file(WRITE ${CMAKE_BINARY_DIR}/ffmpeg_fixpaths.cmake
"foreach(f Makefile ffbuild/config.mak)
    file(READ \"\${DIR}/\${f}\" c)
    string(REPLACE \"${FFMPEG_SRC_MSYS}\" \"${FFMPEG_SRC}\" c \"\${c}\")
    file(WRITE \"\${DIR}/\${f}\" \"\${c}\")
endforeach()
")
        # 最小 LGPL 组件集（本地播放；网络协议后续按需追加）
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
        # 导入目标：头文件双根——源码树（libav*/*.h）+ 构建目录（config.h / version.h）
        foreach(ff_lib avformat avcodec swscale swresample avutil)
            add_library(ff_${ff_lib} STATIC IMPORTED GLOBAL)
            set_target_properties(ff_${ff_lib} PROPERTIES
                IMPORTED_LOCATION ${FFMPEG_BUILD}/lib${ff_lib}/lib${ff_lib}.a
                INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_BUILD};${FFMPEG_SRC}"
                INTERFACE_LINK_LIBRARIES "ws2_32;bcrypt;ole32")  # Windows 静态链接系统依赖
            add_dependencies(ff_${ff_lib} ffmpeg_ext)
        endforeach()
    else()
        message(STATUS "FFmpeg 后端：未找到 Git bash / mingw32-make，降级为 Null 播放器")
    endif()
endif()