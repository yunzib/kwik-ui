# Diff Details

Date : 2026-06-17 22:04:20

Directory c:\\ws-code\\ws-kwik\\kwik-ui

Total : 116 files,  9845 codes, 1367 comments, 998 blanks, all 12210 lines

[Summary](results.md) / [Details](details.md) / [Diff Summary](diff.md) / Diff Details

## Files
| filename | language | code | comment | blank | total |
| :--- | :--- | ---: | ---: | ---: | ---: |
| [CMakeLists.txt](/CMakeLists.txt) | CMake | 7 | 0 | 2 | 9 |
| [README.md](/README.md) | Markdown | 109 | 0 | 0 | 109 |
| [cmake/modules/Bridge.cmake](/cmake/modules/Bridge.cmake) | CMake | 2 | 0 | 0 | 2 |
| [cmake/modules/Element.cmake](/cmake/modules/Element.cmake) | CMake | 19 | 0 | 1 | 20 |
| [cmake/modules/Render.cmake](/cmake/modules/Render.cmake) | CMake | 14 | 0 | 1 | 15 |
| [doc/1. 提示词.md](/doc/1.%20%E6%8F%90%E7%A4%BA%E8%AF%8D.md) | Markdown | -56 | 0 | -2 | -58 |
| [doc/1.kwik-ui 组件.md](/doc/1.kwik-ui%20%E7%BB%84%E4%BB%B6.md) | Markdown | 277 | 0 | 24 | 301 |
| [doc/Text渲染流程.md](/doc/Text%E6%B8%B2%E6%9F%93%E6%B5%81%E7%A8%8B.md) | Markdown | -156 | 0 | 0 | -156 |
| [doc/事件处理流程.md](/doc/%E4%BA%8B%E4%BB%B6%E5%A4%84%E7%90%86%E6%B5%81%E7%A8%8B.md) | Markdown | -193 | 0 | 0 | -193 |
| [doc/渲染流程.md](/doc/%E6%B8%B2%E6%9F%93%E6%B5%81%E7%A8%8B.md) | Markdown | -109 | 0 | -5 | -114 |
| [examples/CMakeLists.txt](/examples/CMakeLists.txt) | CMake | 2 | 0 | 3 | 5 |
| [examples/checkbox.js](/examples/checkbox.js) | JavaScript | 38 | 2 | 0 | 40 |
| [examples/dropdown.js](/examples/dropdown.js) | JavaScript | 65 | 3 | 4 | 72 |
| [examples/example.cpp](/examples/example.cpp) | C++ | 7 | 0 | 0 | 7 |
| [examples/example.js](/examples/example.js) | JavaScript | 408 | 29 | 0 | 437 |
| [examples/example1.js](/examples/example1.js) | JavaScript | 248 | 20 | 0 | 268 |
| [examples/image.js](/examples/image.js) | JavaScript | 133 | 7 | 0 | 140 |
| [examples/image/Web Analytics.svg](/examples/image/Web%20Analytics.svg) | XML | 1 | 0 | 0 | 1 |
| [examples/image/Web Application.svg](/examples/image/Web%20Application.svg) | XML | 1 | 0 | 0 | 1 |
| [examples/image/home.svg](/examples/image/home.svg) | XML | 1 | 0 | 0 | 1 |
| [examples/image/查询统计.svg](/examples/image/%E6%9F%A5%E8%AF%A2%E7%BB%9F%E8%AE%A1.svg) | XML | 1 | 0 | 0 | 1 |
| [examples/image/系统管理.svg](/examples/image/%E7%B3%BB%E7%BB%9F%E7%AE%A1%E7%90%86.svg) | XML | 1 | 0 | 0 | 1 |
| [examples/image/菜单 (1).svg](/examples/image/%E8%8F%9C%E5%8D%95%20(1).svg) | XML | 1 | 0 | 0 | 1 |
| [examples/image/菜单.svg](/examples/image/%E8%8F%9C%E5%8D%95.svg) | XML | 1 | 0 | 0 | 1 |
| [examples/input.js](/examples/input.js) | JavaScript | 84 | 19 | 0 | 103 |
| [examples/radiobutton.js](/examples/radiobutton.js) | JavaScript | 32 | 2 | 3 | 37 |
| [examples/stack.js](/examples/stack.js) | JavaScript | 18 | 0 | 0 | 18 |
| [examples/text.js](/examples/text.js) | JavaScript | 4 | 0 | 0 | 4 |
| [examples/textarea.js](/examples/textarea.js) | JavaScript | 104 | 6 | 2 | 112 |
| [examples/view.js](/examples/view.js) | JavaScript | 8 | 1 | 3 | 12 |
| [examples/vk\_resize\_test.cpp](/examples/vk_resize_test.cpp) | C++ | 146 | 12 | 20 | 178 |
| [examples/win32\_window\_example.cpp](/examples/win32_window_example.cpp) | C++ | 18 | -2 | 1 | 17 |
| [include/svg\_decoder.h](/include/svg_decoder.h) | C++ | 12 | 14 | 1 | 27 |
| [modules/app/application.cppm](/modules/app/application.cppm) | C++ | 3 | 9 | 2 | 14 |
| [modules/bridge/prop\_bus.cppm](/modules/bridge/prop_bus.cppm) | C++ | 4 | 6 | 1 | 11 |
| [modules/bridge/props\_parser.cppm](/modules/bridge/props_parser.cppm) | C++ | 6 | 0 | 6 | 12 |
| [modules/core/types.cppm](/modules/core/types.cppm) | C++ | 19 | 9 | 3 | 31 |
| [modules/element/button.cppm](/modules/element/button.cppm) | C++ | -5 | 0 | 2 | -3 |
| [modules/element/checkbox.cppm](/modules/element/checkbox.cppm) | C++ | 41 | 15 | 4 | 60 |
| [modules/element/dropdown.cppm](/modules/element/dropdown.cppm) | C++ | 51 | 16 | 7 | 74 |
| [modules/element/image.cppm](/modules/element/image.cppm) | C++ | 5 | 0 | 1 | 6 |
| [modules/element/input.cppm](/modules/element/input.cppm) | C++ | 73 | 28 | 4 | 105 |
| [modules/element/props.cppm](/modules/element/props.cppm) | C++ | 67 | 16 | 6 | 89 |
| [modules/element/radiobutton.cppm](/modules/element/radiobutton.cppm) | C++ | 40 | 5 | 6 | 51 |
| [modules/element/text.cppm](/modules/element/text.cppm) | C++ | 0 | 0 | 2 | 2 |
| [modules/element/textarea.cppm](/modules/element/textarea.cppm) | C++ | 59 | 17 | 5 | 81 |
| [modules/element/view.cppm](/modules/element/view.cppm) | C++ | 110 | 91 | 19 | 220 |
| [modules/engine/quickjs\_context.cppm](/modules/engine/quickjs_context.cppm) | C++ | 22 | 22 | 3 | 47 |
| [modules/event/event.cppm](/modules/event/event.cppm) | C++ | 3 | 1 | 2 | 6 |
| [modules/layout/flex\_layout.cppm](/modules/layout/flex_layout.cppm) | C++ | 1 | 0 | 0 | 1 |
| [modules/layout/grid\_layout.cppm](/modules/layout/grid_layout.cppm) | C++ | 3 | 0 | 1 | 4 |
| [modules/layout/list\_layout.cppm](/modules/layout/list_layout.cppm) | C++ | 4 | 0 | 1 | 5 |
| [modules/layout/radio\_group.cppm](/modules/layout/radio_group.cppm) | C++ | 26 | 11 | 4 | 41 |
| [modules/layout/stack\_layout.cppm](/modules/layout/stack_layout.cppm) | C++ | 3 | 0 | 0 | 3 |
| [modules/platform/window.cppm](/modules/platform/window.cppm) | C++ | 2 | 0 | 0 | 2 |
| [modules/render/backend.cppm](/modules/render/backend.cppm) | C++ | 0 | -1 | 0 | -1 |
| [modules/render/command.cppm](/modules/render/command.cppm) | C++ | 6 | 10 | -12 | 4 |
| [modules/render/font.cppm](/modules/render/font.cppm) | C++ | 19 | 18 | 5 | 42 |
| [modules/render/graphics.cppm](/modules/render/graphics.cppm) | C++ | 0 | 1 | 0 | 1 |
| [modules/render/render\_thread.cppm](/modules/render/render_thread.cppm) | C++ | -9 | -19 | -4 | -32 |
| [modules/render/software\_backend.cppm](/modules/render/software_backend.cppm) | C++ | -3 | 3 | 0 | 0 |
| [modules/render/vulkan/vulkan\_backend.cppm](/modules/render/vulkan/vulkan_backend.cppm) | C++ | 59 | 10 | 3 | 72 |
| [modules/render/vulkan/vulkan\_clip\_manager.cppm](/modules/render/vulkan/vulkan_clip_manager.cppm) | C++ | 27 | 0 | 2 | 29 |
| [modules/render/vulkan/vulkan\_context.cppm](/modules/render/vulkan/vulkan_context.cppm) | C++ | 110 | 7 | 12 | 129 |
| [modules/render/vulkan/vulkan\_glyph\_renderer.cppm](/modules/render/vulkan/vulkan_glyph_renderer.cppm) | C++ | 32 | 0 | 3 | 35 |
| [modules/render/vulkan/vulkan\_image\_renderer.cppm](/modules/render/vulkan/vulkan_image_renderer.cppm) | C++ | 39 | 0 | 3 | 42 |
| [modules/render/vulkan/vulkan\_rect\_renderer.cppm](/modules/render/vulkan/vulkan_rect_renderer.cppm) | C++ | 33 | 0 | 4 | 37 |
| [modules/render/vulkan\_backend.cppm](/modules/render/vulkan_backend.cppm) | C++ | -133 | 138 | 0 | 5 |
| [shaders/glyph.frag](/shaders/glyph.frag) | GLSL | 1 | 7 | 0 | 8 |
| [shaders/image.frag](/shaders/image.frag) | GLSL | 17 | 5 | 5 | 27 |
| [shaders/image.vert](/shaders/image.vert) | GLSL | 5 | 0 | 0 | 5 |
| [src/app/application.cpp](/src/app/application.cpp) | C++ | 69 | 19 | 26 | 114 |
| [src/bridge/element\_parser.cpp](/src/bridge/element_parser.cpp) | C++ | 31 | 4 | 7 | 42 |
| [src/bridge/prop\_bus.cpp](/src/bridge/prop_bus.cpp) | C++ | 58 | 3 | 1 | 62 |
| [src/bridge/props\_parser.cpp](/src/bridge/props_parser.cpp) | C++ | 115 | 21 | 7 | 143 |
| [src/element/button.cpp](/src/element/button.cpp) | C++ | 4 | -3 | 1 | 2 |
| [src/element/checkbox.cpp](/src/element/checkbox.cpp) | C++ | 107 | 28 | 0 | 135 |
| [src/element/dropdown.cpp](/src/element/dropdown.cpp) | C++ | 226 | 40 | 26 | 292 |
| [src/element/image.cpp](/src/element/image.cpp) | C++ | 39 | 18 | 0 | 57 |
| [src/element/input.cpp](/src/element/input.cpp) | C++ | 328 | 35 | 6 | 369 |
| [src/element/radiobutton.cpp](/src/element/radiobutton.cpp) | C++ | 99 | 24 | 1 | 124 |
| [src/element/svg\_decoder.cpp](/src/element/svg_decoder.cpp) | C++ | 51 | 20 | 1 | 72 |
| [src/element/text.cpp](/src/element/text.cpp) | C++ | 9 | 3 | 2 | 14 |
| [src/element/textarea.cpp](/src/element/textarea.cpp) | C++ | 308 | 56 | 3 | 367 |
| [src/element/view.cpp](/src/element/view.cpp) | C++ | 131 | 22 | 12 | 165 |
| [src/engine/bindings.cpp](/src/engine/bindings.cpp) | C++ | 43 | 0 | 7 | 50 |
| [src/engine/quickjs\_context.cpp](/src/engine/quickjs_context.cpp) | C++ | 14 | 2 | 0 | 16 |
| [src/engine/quickjs\_runtime.cpp](/src/engine/quickjs_runtime.cpp) | C++ | 1 | 0 | 0 | 1 |
| [src/event/event.cpp](/src/event/event.cpp) | C++ | 6 | 1 | 2 | 9 |
| [src/layout/radio\_group.cpp](/src/layout/radio_group.cpp) | C++ | 24 | 39 | 0 | 63 |
| [src/layout/stack\_layout.cpp](/src/layout/stack_layout.cpp) | C++ | 22 | 3 | 0 | 25 |
| [src/platform/window\_win32.cpp](/src/platform/window_win32.cpp) | C++ | 1 | 1 | 1 | 3 |
| [src/render/command.cpp](/src/render/command.cpp) | C++ | -12 | 7 | -24 | -29 |
| [src/render/font.cpp](/src/render/font.cpp) | C++ | 4 | 6 | 3 | 13 |
| [src/render/graphics.cpp](/src/render/graphics.cpp) | C++ | -11 | 2 | -2 | -11 |
| [src/render/render\_thread.cpp](/src/render/render_thread.cpp) | C++ | -33 | -5 | -12 | -50 |
| [src/render/software\_backend.cpp](/src/render/software_backend.cpp) | C++ | -79 | -14 | -22 | -115 |
| [src/render/vulkan/vulkan\_backend.cpp](/src/render/vulkan/vulkan_backend.cpp) | C++ | 137 | 9 | 5 | 151 |
| [src/render/vulkan/vulkan\_clip\_manager.cpp](/src/render/vulkan/vulkan_clip_manager.cpp) | C++ | 58 | 0 | 2 | 60 |
| [src/render/vulkan/vulkan\_context.cpp](/src/render/vulkan/vulkan_context.cpp) | C++ | 782 | 82 | 49 | 913 |
| [src/render/vulkan/vulkan\_glyph\_renderer.cpp](/src/render/vulkan/vulkan_glyph_renderer.cpp) | C++ | 389 | 27 | 1 | 417 |
| [src/render/vulkan/vulkan\_image\_renderer.cpp](/src/render/vulkan/vulkan_image_renderer.cpp) | C++ | 443 | 28 | 3 | 474 |
| [src/render/vulkan/vulkan\_rect\_renderer.cpp](/src/render/vulkan/vulkan_rect_renderer.cpp) | C++ | 299 | 20 | 2 | 321 |
| [src/render/vulkan\_backend.cpp](/src/render/vulkan_backend.cpp) | C++ | -1,296 | -144 | -27 | -1,467 |
| [third\_party/nanosvg/AI\_POLICY.md](/third_party/nanosvg/AI_POLICY.md) | Markdown | 22 | 0 | 8 | 30 |
| [third\_party/nanosvg/CMakeLists.txt](/third_party/nanosvg/CMakeLists.txt) | CMake | 59 | 0 | 16 | 75 |
| [third\_party/nanosvg/README.md](/third_party/nanosvg/README.md) | Markdown | 78 | 0 | 35 | 113 |
| [third\_party/nanosvg/example/23.svg](/third_party/nanosvg/example/23.svg) | XML | 729 | 0 | 2 | 731 |
| [third\_party/nanosvg/example/drawing.svg](/third_party/nanosvg/example/drawing.svg) | XML | 95 | 1 | 2 | 98 |
| [third\_party/nanosvg/example/example1.c](/third_party/nanosvg/example/example1.c) | C | 198 | 24 | 37 | 259 |
| [third\_party/nanosvg/example/example2.c](/third_party/nanosvg/example/example2.c) | C | 43 | 17 | 10 | 70 |
| [third\_party/nanosvg/example/nano.svg](/third_party/nanosvg/example/nano.svg) | XML | 26 | 1 | 1 | 28 |
| [third\_party/nanosvg/example/stb\_image\_write.h](/third_party/nanosvg/example/stb_image_write.h) | C++ | 388 | 73 | 51 | 512 |
| [third\_party/nanosvg/premake4.lua](/third_party/nanosvg/premake4.lua) | Lua | 42 | 0 | 15 | 57 |
| [third\_party/nanosvg/src/nanosvg.h](/third_party/nanosvg/src/nanosvg.h) | C++ | 2,712 | 195 | 371 | 3,278 |
| [third\_party/nanosvg/src/nanosvgrast.h](/third_party/nanosvg/src/nanosvgrast.h) | C++ | 1,098 | 164 | 211 | 1,473 |

[Summary](results.md) / [Details](details.md) / [Diff Summary](diff.md) / Diff Details