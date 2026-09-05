# ── 编译工具本身（始终构建，不依赖构建类型）──
add_executable(compile_js_bundle compile_js_bundle.cpp)
target_include_directories(compile_js_bundle PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(compile_js_bundle PRIVATE qjs qjs-libc)

# ── 安装到库的 bin/ 目录 ──
install(TARGETS compile_js_bundle
        EXPORT kwik-ui-targets
        RUNTIME DESTINATION bin)