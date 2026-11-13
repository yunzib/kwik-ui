module;
// GCC16 模块边界守卫：C 标准头直取 mingw-w64 纯 C 版本（include_next），
// 绕开 libstdc++ 的 C++ 包装头——否则 windows.h→stdlib.h→mm_malloc→xmmintrin
// 的文本链与 import std 的同名声明产生语言链接冲突
#define _GLIBCXX_INCLUDE_NEXT_C_HEADERS 1
// 内在函数头同因：mingw intrin.h 将 x86intrin 包进 extern "C"（文本侧 C 链接），
// 与 std 模块内 C++ 链接的同名声明冲突——预置 guard 使文本路径落空（声明仍由
// import std 提供，用户代码无感）
#define _MM_MALLOC_H_INCLUDED
#define _X86INTRIN_H_INCLUDED
#define _IMMINTRIN_H_INCLUDED
#define _XMMINTRIN_H_INCLUDED
#define _EMMINTRIN_H_INCLUDED
#define _MMINTRIN_H_INCLUDED
#define _PMMINTRIN_H_INCLUDED
#define _CPUID_H_INCLUDED
#include <vulkan/vulkan.h>
export module kwik.render.vulkan.rect_renderer;
import std;
import kwik.core.types;

export class RectRenderer {
public:
    RectRenderer() = default;
    ~RectRenderer();
    bool create(VkDevice device, VkPipelineCache cache, VkRenderPass renderPass, VkBuffer vertexBuffer, VkBuffer indexBuffer);
    void destroy();

    void clear(VkCommandBuffer cmd, VkExtent2D extent, const Color &color);
    void fillRect(VkCommandBuffer cmd, VkExtent2D extent, const Rect &rect, const Color &color, const Transform2D &t);
    void fillRoundedRect(VkCommandBuffer cmd, VkExtent2D extent, const Rect &rect, float radius, const Color &color,
                         float globalAlpha, const Transform2D &t);
    void fillRoundedRect(VkCommandBuffer cmd, VkExtent2D extent, const Rect &rect, float radius, const Color &color,
                         const Gradient &gradient, float globalAlpha, const Transform2D &t);
    /** @brief 线段胶囊描边 */
    void drawSegment(VkCommandBuffer cmd, VkExtent2D extent, float ax, float ay, float bx, float by, float halfW,
                     const Color &color, float globalAlpha, const Transform2D &t);
    void strokeRoundedRect(VkCommandBuffer cmd, VkExtent2D extent, const Rect &rect, float radius, const Color &color,
                           float strokeWidth, float globalAlpha, const Transform2D &t);
    void drawShadow(VkCommandBuffer cmd, VkExtent2D extent, const Rect &rect, float radius, const Shadow &shadow,
                    float globalAlpha, const Transform2D &t);
    void writeStencilMask(VkCommandBuffer cmd, VkExtent2D extent, const Rect &rect, float radius, const Transform2D &t);
    void disableStencilTest(VkCommandBuffer cmd);

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline fillPipeline_ = VK_NULL_HANDLE;
    VkPipeline strokePipeline_ = VK_NULL_HANDLE;
    VkPipeline shadowPipeline_ = VK_NULL_HANDLE;
    VkPipeline stencilPipeline_ = VK_NULL_HANDLE;
};