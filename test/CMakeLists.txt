# # 假设当前目录是 examples，根目录已包含 QuickJS
# add_executable(test_view view/view.cpp)

# target_link_libraries(test_view PRIVATE
#     quickjs          # 静态库
#     skia
#     core_mod
#     element_mod
#     render_mod
#     engine_mod
# )

# if(Vulkan_FOUND)
#     target_link_libraries(test_view PRIVATE Vulkan::Vulkan)
# endif()
# if(OpenGL_FOUND)
#     target_link_libraries(test_view PRIVATE OpenGL::GL)
# endif()

# # 包含其他必要头文件（QuickJS 的已由目标自动传递）
# target_include_directories(test_view PRIVATE
#     ${CMAKE_SOURCE_DIR}/include
#     ${CMAKE_SOURCE_DIR}/third_party/skia/include
#     ${Vulkan_INCLUDE_DIRS}
#     ${OpenGL_INCLUDE_DIRS}
# )

# # C++ 标准
# if(MSVC)
#     target_compile_options(test_view PRIVATE /std:c++latest /utf-8)
# else()
#     target_compile_options(test_view PRIVATE -std=c++26)
# endif()


# ==================== 示例程序配置 ====================
if(KWIK_BUILD_EXAMPLES)
    # 窗口示例
    # add_executable(example_window examples/window_example.cpp)
    # target_link_libraries(example_window PRIVATE kwik_mod)
    
    # # 添加平台定义
    # target_compile_definitions(example_window
    #     PRIVATE
    #         $<$<BOOL:${KWIK_USE_VULKAN}>:KWIK_USE_VULKAN>
    #         $<$<BOOL:${KWIK_USE_SOFTWARE}>:KWIK_USE_SOFTWARE>
    # )
    
    # # 复制资源文件（如果存在）
    # if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/resources)
    #     file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/examples/resources
    #          DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/examples)
    # endif()

    # win32 window example
    # add_executable(win32_window_example ${CMAKE_SOURCE_DIR}/examples/win32_window_example.cpp)
    # target_link_libraries(win32_window_example PRIVATE kwik)

    # add_executable(vk_resize_test ${CMAKE_SOURCE_DIR}/examples/vk_resize_test.cpp)
    # target_link_libraries(vk_resize_test PRIVATE kwik)

    # example
    add_executable(example ${CMAKE_SOURCE_DIR}/test/example.cpp)
    target_link_libraries(example PRIVATE kwik)
    # 图表扩展开启时启用示例的 chart/gauge demo 分支（KWIK_ENABLE_CHART=OFF 时不编译）
    if(TARGET kwik_ext_chart)
        target_compile_definitions(example PRIVATE KWIK_HAVE_CHART)
    endif()
    # 视频/3D 扩展同口径：目标不存在时 import 与注册调用一并裁掉
    # （KWIK_ENABLE_VIDEO/G3D=OFF 裁剪变体首次可完整构建，与 chart 模式统一）
    if(TARGET kwik_ext_video)
        target_compile_definitions(example PRIVATE KWIK_HAVE_VIDEO)
    endif()
    if(TARGET kwik_ext_g3d)
        target_compile_definitions(example PRIVATE KWIK_HAVE_G3D)
    endif()

    # kwik_unit_tests — 纯逻辑单测（不依赖窗口/GPU）：Rect / PropMeta 一致性 / DisplayList / 事件代理对重组
    # enable_testing() 在顶层 CMakeLists（ctest 只发现顶层注册的用例）
    add_executable(kwik_unit_tests unit/core_tests.cpp)
    target_link_libraries(kwik_unit_tests PRIVATE kwik_element kwik_core kwik_event)
    add_test(NAME core_tests COMMAND kwik_unit_tests)

    # chart_hit_probe — Chart 命中几何探针（无窗口/GPU）：renderer hitTest 的
    # frame 域坐标口径锁（D 批次悬停错位回归）。依赖为 PRIVATE 链接不下发，
    # 模块 BMI 可达性需显式补齐（同 kwik_js_tests 口径）。
    add_executable(chart_hit_probe unit/chart_hit_probe.cpp)
    target_link_libraries(chart_hit_probe PRIVATE kwik_ext_chart kwik_element kwik_core kwik_event kwik_render)
    add_test(NAME chart_hit_probe COMMAND chart_hit_probe)

    # path_fill_probe — triangulateFill ear clipping 剖分探针（无窗口/GPU）：
    # 凹多边形/星形/CW 环绕/圆弧/共线点/多轮廓的面积守恒 + edgeMask 轮廓边计数锁
    add_executable(path_fill_probe unit/path_fill_probe.cpp)
    target_link_libraries(path_fill_probe PRIVATE kwik_core)
    add_test(NAME path_fill_probe COMMAND path_fill_probe)

    # sector_fill_probe — 饼图扇形覆盖缺口探针（无窗口/GPU）：
    # 示例④ moveTo圆心→arc→closePath→fill 的软件栅格化覆盖锁
    add_executable(sector_fill_probe unit/sector_fill_probe.cpp)
    target_link_libraries(sector_fill_probe PRIVATE kwik_core)
    add_test(NAME sector_fill_probe COMMAND sector_fill_probe)

    # kwik_js_tests — T0-00 headless JS 桥接行为锁：QuickJSContext 脱窗实例化 +
    # 临时模块文件 evalFile，不需要窗口/根 View。bridge/engine 的依赖为
    # PRIVATE 链接不下发，测试目标需显式补齐（模块 BMI 传递可达性）。
    add_executable(kwik_js_tests unit/js_tests.cpp)
    target_link_libraries(kwik_js_tests PRIVATE
        kwik_bridge       # State / 绑定层模块
        kwik_engine       # QuickJSContext / vm_callbacks
        kwik_element kwik_core kwik_event
        qjs qjs-libc      # quickjs.h 头与库（engine 亦为 PRIVATE 链接）
    )
    # Video 扩展开启时补链并启用其行为锁（KWIK_ENABLE_VIDEO=OFF 时跳过，不破构建）
    if(TARGET kwik_ext_video)
        target_link_libraries(kwik_js_tests PRIVATE kwik_ext_video kwik_render)
        target_compile_definitions(kwik_js_tests PRIVATE KWIK_TEST_VIDEO)
    endif()
    # Chart 扩展开启时补链并启用其行为锁（KWIK_ENABLE_CHART=OFF 时跳过，不破构建）
    if(TARGET kwik_ext_chart)
        target_link_libraries(kwik_js_tests PRIVATE kwik_ext_chart kwik_render)
        target_compile_definitions(kwik_js_tests PRIVATE KWIK_TEST_CHART)
    endif()
    add_test(NAME js_tests COMMAND kwik_js_tests)

    # shaper_probe — 诊断探针（不入测试集）：TextShaper 对 \n / \r\n 塑形实况
    add_executable(shaper_probe unit/shaper_probe.cpp)
    target_link_libraries(shaper_probe PRIVATE kwik_render kwik_core)

     # vk_smoke — 裸 Vulkan 内存冒烟测试（诊断工具, 不链接 kwik, 仅 Win32）
    if(WIN32)
        add_executable(vk_smoke ${CMAKE_CURRENT_SOURCE_DIR}/vk_smoke.cpp)
        find_package(Vulkan REQUIRED)          # 本地查找, 不依赖 Render.cmake 的启用状态
        target_link_libraries(vk_smoke PRIVATE Vulkan::Vulkan user32 gdi32)
    endif()

endif()