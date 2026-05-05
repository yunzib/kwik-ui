@echo off
REM 使用 Vulkan SDK 的 glslangValidator 编译着色器
REM glslangValidator 在 Vulkan SDK 安装目录的 Bin 下
glslangValidator -V rect.vert -o rect.vert.spv
glslangValidator -V rect.frag -o rect.frag.spv
echo Done. Copy rect.vert.spv and rect.frag.spv to the build output directory.