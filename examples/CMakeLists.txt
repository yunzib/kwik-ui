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

    # view example
    add_executable(view_example ${CMAKE_SOURCE_DIR}/examples/view/view_example.cpp)
    target_link_libraries(view_example PRIVATE kwik)

    # text example
    add_executable(text_example ${CMAKE_SOURCE_DIR}/examples/text/text_example.cpp)
    target_link_libraries(text_example PRIVATE kwik)

endif()