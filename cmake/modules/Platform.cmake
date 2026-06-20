
# 平台后端选项（用户可设置）
set(KWIK_PLATFORM_BACKEND "auto" CACHE STRING "Platform window backend")
set_property(CACHE KWIK_PLATFORM_BACKEND PROPERTY STRINGS auto win32 wayland x11 drm fbdev android cocoa)

# 自动检测或使用用户指定的后端
if(KWIK_PLATFORM_BACKEND STREQUAL "auto")
    if(WIN32)
        set(KWIK_PLATFORM_BACKEND "win32")
    elseif(ANDROID)
        set(KWIK_PLATFORM_BACKEND "android")
    elseif(APPLE)
        if(IOS)
            set(KWIK_PLATFORM_BACKEND "ios")
        else()
            set(KWIK_PLATFORM_BACKEND "cocoa")
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Linux自动检测：优先Wayland，否则X11
        if(EXISTS "/run/wayland/wayland-0" OR EXISTS "$ENV{XDG_RUNTIME_DIR}/wayland-0")
            set(KWIK_PLATFORM_BACKEND "wayland")
        else()
            set(KWIK_PLATFORM_BACKEND "x11")
        endif()
    else()
        message(FATAL_ERROR "无法自动检测平台后端")
    endif()
endif()

# 根据选定的后端设置具体文件、依赖和编译定义
set(KWIK_PLATFORM_MODULE_FILE "")
set(KWIK_PLATFORM_SOURCE_FILE "")
set(KWIK_PLATFORM_LIBS "")

if(KWIK_PLATFORM_BACKEND STREQUAL "win32")
    set(KWIK_PLATFORM_MODULE_FILE "modules/platform/window_win32.cppm")
    set(KWIK_PLATFORM_SOURCE_FILE "src/platform/window_win32.cpp")
    set(KWIK_PLATFORM_LIBS user32 gdi32)

elseif(KWIK_PLATFORM_BACKEND STREQUAL "wayland")
    set(KWIK_PLATFORM_MODULE_FILE "modules/platform/window_wayland.cppm")
    set(KWIK_PLATFORM_SOURCE_FILE "src/platform/window_wayland.cpp")
    find_package(Wayland REQUIRED COMPONENTS client cursor)
    find_package(xkbcommon REQUIRED)
    set(KWIK_PLATFORM_LIBS Wayland::client Wayland::cursor ${xkbcommon_LIBRARIES})

elseif(KWIK_PLATFORM_BACKEND STREQUAL "x11")
    set(KWIK_PLATFORM_MODULE_FILE "modules/platform/window_x11.cppm")
    set(KWIK_PLATFORM_SOURCE_FILE "src/platform/window_x11.cpp")
    find_package(X11 REQUIRED)
    set(KWIK_PLATFORM_LIBS ${X11_LIBRARIES})

elseif(KWIK_PLATFORM_BACKEND STREQUAL "drm")
    set(KWIK_PLATFORM_MODULE_FILE "modules/platform/window_drm.cppm")
    set(KWIK_PLATFORM_SOURCE_FILE "src/platform/window_drm.cpp")
    find_package(LibDRM REQUIRED)
    find_package(GBM REQUIRED)
    find_package(EGL REQUIRED)
    set(KWIK_PLATFORM_LIBS ${LibDRM_LIBRARIES} ${GBM_LIBRARIES} ${EGL_LIBRARIES})

elseif(KWIK_PLATFORM_BACKEND STREQUAL "fbdev")
    set(KWIK_PLATFORM_MODULE_FILE "modules/platform/window_fbdev.cppm")
    set(KWIK_PLATFORM_SOURCE_FILE "src/platform/window_fbdev.cpp")
    # FBDev 不需要额外库

elseif(KWIK_PLATFORM_BACKEND STREQUAL "android")
    set(KWIK_PLATFORM_MODULE_FILE "modules/platform/window_android.cppm")
    set(KWIK_PLATFORM_SOURCE_FILE "src/platform/window_android.cpp")
    set(KWIK_PLATFORM_LIBS log android EGL GLESv2)

elseif(KWIK_PLATFORM_BACKEND STREQUAL "cocoa")
    # macOS 后端（需要额外处理，原函数未完整，这里补充基本框架）
    set(KWIK_PLATFORM_MODULE_FILE "modules/platform/window_cocoa.cppm")
    set(KWIK_PLATFORM_SOURCE_FILE "src/platform/window_cocoa.cpp")
    find_package(OpenGL REQUIRED)  # Cocoa 通常与 OpenGL 配合
    set(KWIK_PLATFORM_LIBS ${OPENGL_LIBRARIES} "-framework Cocoa" "-framework QuartzCore")

elseif(KWIK_PLATFORM_BACKEND STREQUAL "ios")
    set(KWIK_PLATFORM_MODULE_FILE "modules/platform/window_ios.cppm")
    set(KWIK_PLATFORM_SOURCE_FILE "src/platform/window_ios.cpp")
    set(KWIK_PLATFORM_LIBS "-framework UIKit" "-framework OpenGLES" "-framework QuartzCore")

else()
    message(FATAL_ERROR "不支持的平台后端: ${KWIK_PLATFORM_BACKEND}")
endif()

message(STATUS "平台后端: ${KWIK_PLATFORM_BACKEND}")

# ========== 创建 kwik_platform 库 ==========

# 1. 创建库目标
add_library(kwik_platform)

# 2. 添加公共模块接口文件（包含固定模块 + 平台特定模块）
set(PLATFORM_PUBLIC_MODULES
    modules/platform/window.cppm
    modules/platform/window_factory.cppm
    modules/platform/platform.cppm
    ${KWIK_PLATFORM_MODULE_FILE}
)
target_sources(kwik_platform
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules/platform
        FILES ${PLATFORM_PUBLIC_MODULES}
)

# 3. 添加私有实现源文件
set(PLATFORM_PRIVATE_SOURCES
    # src/platform/window_factory.cpp
    ${KWIK_PLATFORM_SOURCE_FILE}
)
target_sources(kwik_platform
    PRIVATE
        ${PLATFORM_PRIVATE_SOURCES}
)

# 5. 添加依赖（kwik_core）和链接库（平台特定库）
target_link_libraries(kwik_platform
    PRIVATE
        kwik_utils
        kwik_core
        ${KWIK_PLATFORM_LIBS}
)

# 6. 添加编译定义
target_compile_definitions(kwik_platform
    PRIVATE
        KWIK_PLATFORM_${KWIK_PLATFORM_BACKEND}
)

install(TARGETS kwik_platform
    EXPORT KwiKUITargets
    FILE_SET cxx_modules DESTINATION share/kwik-ui/modules/platform
    ARCHIVE DESTINATION lib
)