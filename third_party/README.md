> 本仓库采用**全量保留**快照模式：除 .git 元数据外不裁剪任何文件（tests/docs 等一并保留，
> 便于升级时全量对比）。fastgltf/deps/ 为 simdjson 离线备份副本，构建不消费，保留。

# 三方依赖清单（快照内嵌）

本目录全部三方库以**快照**形式随主仓分发（`git clone` 即得，构建零网络）。
每个库登记：上游地址 / 版本 / 许可 / 提取规则（Files extracted）/ 补丁清单。
**升级任何库必须走 `scripts/vendor_update.ps1`**（按下方提取规则重建子集后重放补丁），
禁止手工混入未登记文件。新版本升级后更新对应 Version 行。

## quickjs
- Upstream: https://github.com/quickjs-ng/quickjs
- Version: git (130081a, 2026-03-22)
- License: MIT
- Files extracted: 根源码 *.c/*.h（含 quickjs.h）+ cmake/ + CMakeLists.txt；
  排除 tests/ examples/ docs/ benchmarks/ .github/
- Patches: 0001-llvm-mingw-msvc-compat.patch
  （Clang+Win 非 MSVC 命令行关 CRT 安全告警；Clang+MSVC-ABI 补 -Xlinker /STACK:8388608）

## freetype
- Upstream: https://github.com/freetype/freetype
- Version: git (b6bcd2177, 2026-05-03)
- License: FTL + GPLv2（两许可文件已拷至库根）
- Files extracted: include/ + src/ + CMakeLists.txt + FTL.TXT + GPLv2.TXT；
  排除 test/ builds/ devel/ .github/ docs/
- Patches: 无

## harfbuzz
- Upstream: https://github.com/harfbuzz/harfbuzz
- Version: git (5ee942feb, 2026-05-04)
- License: MIT (COPYING)
- Files extracted: src/ 全量 + CMakeLists.txt + COPYING；
  排除 test/ docs/ util/ perf/ .github/
- Patches: 0001-cmake-link-visibility.patch
  （THIRD_PARTY_LIBS 转 PRIVATE 收敛泄漏；freetype 转 PUBLIC 修复 hb_ft_*→FT_* 链接可见性）

## stb
- Upstream: https://github.com/nothings/stb
- Version: git (31c1ad3, 2026-04-15)
- License: Public Domain / MIT (LICENSE)
- Files extracted: 仅 stb_image.h + LICENSE（全仓唯一使用头，使用方
  src/element/image.cpp:18 与 src/element/stb_image_impl.cpp:4）
- Patches: 无

## nanosvg
- Upstream: https://github.com/memononen/nanosvg
- Version: git (48120e9, 2026-05-18)
- License: zlib (LICENSE.txt)
- Files extracted: src/nanosvg.h + src/nanosvgrast.h + CMakeLists.txt
  + Config.cmake.in + LICENSE.txt + README.md；排除 example/ tools/
  （src/ 子目录路径保持：Element.cmake 的 include 目录与根 install 规则均指向它）
- Patches: 无

## simdjson
- Upstream: https://github.com/simdjson/simdjson
- Version: v4.6.2 (singleheader)
- License: Apache-2.0 (LICENSE)
- Files extracted: singleheader/simdjson.h + singleheader/simdjson.cpp + LICENSE
- 用途: fastgltf 的 JSON 后端；target 在 Dependencies.cmake 定义，
  使 fastgltf/cmake/dependencies.cmake:4 检测到 TARGET 后跳过 configure 期下载
- 来源: 由 fastgltf/deps/simdjson 离线副本复制（v4.6.2 已核对版本）
- Patches: 无

## FFmpeg
- Upstream: https://github.com/FFmpeg/FFmpeg
- Version: git (3c1aad2, 2026-11-08)（建议升级时 pin 到 release tag 如 n8.0）
- License: LGPL-2.1+（configure 使用 --disable-gpl；COPYING.LGPLv2.1 保留）
- Files extracted: 根 configure + Makefile + VERSION + COPYING.LGPLv2.1
  + ffbuild/ + compat/ + libavformat/ + libavcodec/ + libavutil/
  + libswscale/ + libswresample/ 全量源码；排除 .git/ .github/ doc/ tests/ tools/
  （不删未启用组件的源码——Makefile 组件列表引用，删则升级/重配置碎）
- 构建: 上游无 CMake 后端，由 cmake/modules/FFmpeg.cmake 驱动
  Git bash configure + mingw32-make（工具缺失自动降级 Null 播放器）
- Patches: 无

## fastgltf
- Upstream: https://github.com/spnda/fastgltf
- Version: git (a31be25, 2026-04-20)
- License: MIT (LICENSE.md)
- Files extracted: CMakeLists.txt + cmake/ + include/ + src/ + LICENSE.md + README.md；
  排除 deps/（configure 期下载目录，已由 simdjson target 替代）tests/ examples/ docs/ .git/
- Patches: 无

## msdfgen
- Upstream: https://github.com/Chlumsky/msdfgen
- Version: git (e06c7ea, 2026-05-17)
- License: MIT
- Files extracted: 全量保留（除 .git）
- 状态: 当前未启用（无构建引用，保留源码以备将来启用）
- Patches: 无