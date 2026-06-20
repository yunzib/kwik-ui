@echo off
set SDK_DIR=%~dp0..\..\build\install
cmake -B build -G Ninja -DCMAKE_PREFIX_PATH=%SDK_DIR%
cmake --build build