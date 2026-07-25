@echo off
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Debug
set SDK_DIR=%~dp0..\..\build\install
cmake -B build -G Ninja -DCMAKE_PREFIX_PATH=%SDK_DIR% -DCMAKE_BUILD_TYPE=%CONFIG%
cmake --build build