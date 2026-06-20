# Diff Details

Date : 2026-06-20 18:07:15

Directory c:\\ws-code\\ws-kwik\\kwik-ui

Total : 52 files,  2061 codes, 906 comments, 521 blanks, all 3488 lines

[Summary](results.md) / [Details](details.md) / [Diff Summary](diff.md) / Diff Details

## Files
| filename | language | code | comment | blank | total |
| :--- | :--- | ---: | ---: | ---: | ---: |
| [CMakeLists.txt](/CMakeLists.txt) | CMake | 12 | 0 | 2 | 14 |
| [README.md](/README.md) | Markdown | 45 | 0 | 15 | 60 |
| [cmake/modules/Bridge.cmake](/cmake/modules/Bridge.cmake) | CMake | 2 | 0 | 0 | 2 |
| [cmake/modules/Core.cmake](/cmake/modules/Core.cmake) | CMake | 5 | 0 | 0 | 5 |
| [cmake/modules/Element.cmake](/cmake/modules/Element.cmake) | CMake | 1 | 0 | 1 | 2 |
| [cmake/modules/Engine.cmake](/cmake/modules/Engine.cmake) | CMake | 4 | 0 | 0 | 4 |
| [doc/1.kwik-ui 组件.md](/doc/1.kwik-ui%20%E7%BB%84%E4%BB%B6.md) | Markdown | -4 | 0 | 3 | -1 |
| [doc/2. State和channel.md](/doc/2.%20State%E5%92%8Cchannel.md) | Markdown | 202 | 0 | 36 | 238 |
| [doc/CHANGELOG.md](/doc/CHANGELOG.md) | Markdown | 84 | 0 | 14 | 98 |
| [examples/CMakeLists.txt](/examples/CMakeLists.txt) | CMake | 0 | 0 | -3 | -3 |
| [examples/channel.js](/examples/channel.js) | JavaScript | 169 | 6 | 10 | 185 |
| [examples/checkbox.js](/examples/checkbox.js) | JavaScript | 45 | 3 | 11 | 59 |
| [examples/dropdown.js](/examples/dropdown.js) | JavaScript | 42 | 12 | 4 | 58 |
| [examples/example.cpp](/examples/example.cpp) | C++ | 52 | -8 | 7 | 51 |
| [examples/input.js](/examples/input.js) | JavaScript | 20 | -7 | 7 | 20 |
| [examples/radiobutton.js](/examples/radiobutton.js) | JavaScript | 34 | 12 | 8 | 54 |
| [examples/test.js](/examples/test.js) | JavaScript | 81 | 18 | 7 | 106 |
| [examples/textarea.js](/examples/textarea.js) | JavaScript | 8 | 1 | 5 | 14 |
| [modules/app/application.cppm](/modules/app/application.cppm) | C++ | 7 | 0 | 5 | 12 |
| [modules/bridge/binding\_registry.cppm](/modules/bridge/binding_registry.cppm) | C++ | 35 | 73 | 16 | 124 |
| [modules/bridge/props\_parser.cppm](/modules/bridge/props_parser.cppm) | C++ | 101 | 95 | 17 | 213 |
| [modules/core/coroutine.cppm](/modules/core/coroutine.cppm) | C++ | 89 | 17 | 18 | 124 |
| [modules/core/scheduler.cppm](/modules/core/scheduler.cppm) | C++ | 23 | 35 | 10 | 68 |
| [modules/core/task\_queue.cppm](/modules/core/task_queue.cppm) | C++ | 36 | 33 | 12 | 81 |
| [modules/core/thread\_pool.cppm](/modules/core/thread_pool.cppm) | C++ | 67 | 24 | 14 | 105 |
| [modules/element/checkbox.cppm](/modules/element/checkbox.cppm) | C++ | 1 | 22 | 9 | 32 |
| [modules/element/dropdown.cppm](/modules/element/dropdown.cppm) | C++ | 9 | 0 | 4 | 13 |
| [modules/element/input.cppm](/modules/element/input.cppm) | C++ | 10 | 0 | 4 | 14 |
| [modules/element/radiobutton.cppm](/modules/element/radiobutton.cppm) | C++ | 2 | 1 | 2 | 5 |
| [modules/element/textarea.cppm](/modules/element/textarea.cppm) | C++ | 9 | 0 | 3 | 12 |
| [modules/element/typed\_prop.cppm](/modules/element/typed_prop.cppm) | C++ | 47 | 73 | 11 | 131 |
| [modules/element/view.cppm](/modules/element/view.cppm) | C++ | 3 | 14 | 3 | 20 |
| [modules/engine/bindings.cppm](/modules/engine/bindings.cppm) | C++ | 0 | 23 | 2 | 25 |
| [modules/engine/channel.cppm](/modules/engine/channel.cppm) | C++ | 168 | 72 | 51 | 291 |
| [modules/engine/quickjs\_context.cppm](/modules/engine/quickjs_context.cppm) | C++ | -13 | 8 | 1 | -4 |
| [modules/engine/state\_binding.cppm](/modules/engine/state_binding.cppm) | C++ | 11 | 36 | 8 | 55 |
| [modules/layout/radio\_group.cppm](/modules/layout/radio_group.cppm) | C++ | 5 | 27 | 7 | 39 |
| [src/app/application.cpp](/src/app/application.cpp) | C++ | 13 | 9 | 7 | 29 |
| [src/bridge/binding\_registry.cpp](/src/bridge/binding_registry.cpp) | C++ | 78 | 29 | 25 | 132 |
| [src/bridge/element\_parser.cpp](/src/bridge/element_parser.cpp) | C++ | 70 | 23 | 13 | 106 |
| [src/bridge/props\_parser.cpp](/src/bridge/props_parser.cpp) | C++ | -81 | -2 | 6 | -77 |
| [src/core/scheduler.cpp](/src/core/scheduler.cpp) | C++ | 15 | 4 | 6 | 25 |
| [src/element/checkbox.cpp](/src/element/checkbox.cpp) | C++ | 33 | 11 | 20 | 64 |
| [src/element/dropdown.cpp](/src/element/dropdown.cpp) | C++ | 31 | 0 | 3 | 34 |
| [src/element/input.cpp](/src/element/input.cpp) | C++ | 21 | 1 | 2 | 24 |
| [src/element/radiobutton.cpp](/src/element/radiobutton.cpp) | C++ | 16 | 6 | 2 | 24 |
| [src/element/textarea.cpp](/src/element/textarea.cpp) | C++ | 29 | 0 | 1 | 30 |
| [src/element/view.cpp](/src/element/view.cpp) | C++ | 20 | 0 | 1 | 21 |
| [src/engine/bindings.cpp](/src/engine/bindings.cpp) | C++ | 13 | 143 | 28 | 184 |
| [src/engine/channel.cpp](/src/engine/channel.cpp) | C++ | 294 | 50 | 60 | 404 |
| [src/engine/state\_binding.cpp](/src/engine/state_binding.cpp) | C++ | 32 | 48 | 10 | 90 |
| [src/layout/radio\_group.cpp](/src/layout/radio_group.cpp) | C++ | 65 | -6 | 13 | 72 |

[Summary](results.md) / [Details](details.md) / [Diff Summary](diff.md) / Diff Details