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
export module kwik.render.vulkan.glyph_renderer;
import std;
import kwik.core.types;
import kwik.render.command;
import kwik.render.vulkan.context;
import kwik.render.text.types;


export class GlyphRenderer {
public:
    GlyphRenderer() = default;
    ~GlyphRenderer();
    bool create(VkDevice device, VkPipelineCache cache, VkPhysicalDevice physDevice,
            VkCommandPool cmdPool, VkQueue queue,
            VkRenderPass renderPass,
            VkBuffer vertexBuffer, VkBuffer indexBuffer);
    void destroy();

    void uploadPendingGlyphs(const DeviceContext &dc);
    void drawGlyph(VkCommandBuffer cb, VkExtent2D extent, const DrawGlyphCmd &cmd, float globalAlpha,
                   const Transform2D &t);
    void drawGlyphClipped(VkCommandBuffer cb, VkExtent2D extent, const DrawGlyphCmd &cmd, float globalAlpha,
                          const Transform2D &t);

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkPipeline glyphPipeline_ = VK_NULL_HANDLE;
    VkPipeline glyphClipPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout glyphPipelineLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout glyphClipPipelineLayout_ = VK_NULL_HANDLE;    // clip 变体（工厂各建等价 layout）
    VkDescriptorSetLayout glyphDescSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool glyphDescPool_ = VK_NULL_HANDLE;
    VkDescriptorSet glyphDescSet_ = VK_NULL_HANDLE;
    VkImage glyphAtlasImage_ = VK_NULL_HANDLE;
    VkDeviceMemory glyphAtlasMemory_ = VK_NULL_HANDLE;
    VkImageView glyphAtlasView_ = VK_NULL_HANDLE;
    VkSampler glyphAtlasSampler_ = VK_NULL_HANDLE;
    VkImageLayout atlasLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;  // ← NEW: 跟踪图集 layout
};