# cmake/modules/FFmpeg.cmake — FFmpeg 快照库的 configure+make 构建
#
# FFmpeg 上游无 CMake 后端：本文件在构建期驱动 Git bash 运行 configure、
# 修正路径风格、mingw32-make 编静态库，全部随 cmake --build 一次完成，
# 产物缓存于 build/ffmpeg-build（增量构建跳过）。
# 工具链要求：Git bash（configure 需 sh）+ mingw32-make（主编译器 bin 目录自带）。
# 任一工具缺失 → KWIK_HAVE_FFMPEG=OFF，video 扩展自动降级 Null 播放器。

# ── 选项声明（video 相关开关统一在此，Extension.cmake 只消费）──
option(KWIK_ENABLE_VIDEO "Build video extension" ON)             # OFF=不编 video 插件
option(KWIK_VIDEO_FFMPEG "Build FFmpeg media player backend" ON) # OFF=仅 Null 播放器

set(FFMPEG_SRC   ${CMAKE_SOURCE_DIR}/third_party/FFmpeg)
set(FFMPEG_BUILD ${CMAKE_BINARY_DIR}/ffmpeg-build)
set(KWIK_HAVE_FFMPEG OFF)

if(KWIK_ENABLE_VIDEO AND KWIK_VIDEO_FFMPEG AND EXISTS ${FFMPEG_SRC}/configure)
    # Git bash 解析：Git for Windows 安装目录不固定（本机在 C:/software/Git），
    # 先经 git.exe 反推安装根（本仓构建必有 git）。注意不能让 find_program 落回
    # 系统 PATH——会撞上 C:/Windows/System32/bash.exe（WSL bash，不识别 /c/ 路径，
    # configure 直接 No such file or directory），故一律 NO_DEFAULT_PATH + 显式候选；
    # 找不到即按设计降级 Null 播放器
    # 变量名用 KWIK_FFMPEG_BASH（非常用名）：旧 configure 曾把 WSL bash 缓存进
    # CMakeCache.txt 的 GIT_BASH 条目，find_program 对已存在的缓存条目直接复用——
    # 换名即永久绕开污染缓存，无需用户手工清缓存
    set(KWIK_FFMPEG_BASH "")
    find_program(KWIK_GIT_EXE git)
    if(KWIK_GIT_EXE)
        # git.exe 布局不定：<root>/{cmd,bin,mingw64/bin,usr/bin}——逐级上溯找
        # usr/bin/bash.exe（本机实测 C:/software/Git/mingw64/bin/git.exe，
        # 上溯两级命中 C:/software/Git/usr/bin）
        cmake_path(GET KWIK_GIT_EXE PARENT_PATH _d0)   # git.exe 所在目录
        cmake_path(GET _d0 PARENT_PATH _d1)
        cmake_path(GET _d1 PARENT_PATH _d2)
        foreach(_cand ${_d0}/usr/bin/bash.exe   # git.exe 在 <root>/cmd|bin|usr/bin
                      ${_d1}/usr/bin/bash.exe   # git.exe 在 <root>/mingw64/bin 等
                      ${_d2}/usr/bin/bash.exe)
            if(EXISTS ${_cand})
                set(KWIK_FFMPEG_BASH ${_cand})
                break()
            endif()
        endforeach()
        unset(_cand)
        unset(_d0)
        unset(_d1)
        unset(_d2)
    endif()
    if(NOT KWIK_FFMPEG_BASH)
        # 兜底：常见安装位（NO_DEFAULT_PATH 防 PATH 兜底撞上 System32 的 WSL bash）
        find_program(KWIK_FFMPEG_BASH bash.exe NO_DEFAULT_PATH
            PATHS "C:/Program Files/Git/usr/bin" "C:/Program Files (x86)/Git/usr/bin"
                  "C:/software/Git/usr/bin" "C:/msys64/usr/bin")
    endif()
    # 终检：Git/MSYS bash 的 uname -s 以 MINGW|MSYS 开头；WSL=Linux（或执行失败）→ 视为未找到
    if(KWIK_FFMPEG_BASH)
        execute_process(COMMAND ${KWIK_FFMPEG_BASH} -c "uname -s"
            OUTPUT_VARIABLE _bash_uname OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(NOT _bash_uname MATCHES "^(MINGW|MSYS)")
            set(KWIK_FFMPEG_BASH "")
        endif()
        unset(_bash_uname)
    endif()
    get_filename_component(CC_BIN ${CMAKE_C_COMPILER} DIRECTORY)   # llvm-mingw/bin
    find_program(FFMPEG_MAKE mingw32-make HINTS ${CC_BIN})
    if(KWIK_FFMPEG_BASH AND FFMPEG_MAKE)
        set(KWIK_HAVE_FFMPEG ON)
        get_filename_component(GIT_USR_BIN ${KWIK_FFMPEG_BASH} DIRECTORY)
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
        # FFmpeg 构建工具随主编译器走（configure 的 PATH 已含编译器 bin 目录）：
        # Clang → llvm-* 家族（llvm-mingw）；GCC → binutils 家族（niXman mingw64）
        if(CMAKE_C_COMPILER_ID MATCHES "Clang")
            set(FF_TOOLS --cc=clang --ar=llvm-ar --nm=llvm-nm --ranlib=llvm-ranlib)
        else()
            set(FF_TOOLS --cc=gcc --ar=ar --nm=nm --ranlib=ranlib)
        endif()
        # 最小 LGPL 组件集（本地播放；网络协议后续按需追加）
        set(FFMPEG_FLAGS
            ${FF_TOOLS}
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
            COMMAND ${FFMPEG_ENV} ${KWIK_FFMPEG_BASH} ${FFMPEG_SRC_MSYS}/configure ${FFMPEG_FLAGS}
            COMMAND ${CMAKE_COMMAND} -DDIR=${FFMPEG_BUILD} -P ${CMAKE_BINARY_DIR}/ffmpeg_fixpaths.cmake
            WORKING_DIRECTORY ${FFMPEG_BUILD}
            COMMENT "Configuring FFmpeg (minimal LGPL set, ${CMAKE_C_COMPILER_ID})"
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