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

export module kwik.render.vulkan.backdrop_renderer;
import kwik.render.command;
import kwik.core.types;

import std;

/** @brief 单个 backdrop target（离屏 color，供捕获/模糊） */
export struct BackdropTarget {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSet descSet = VK_NULL_HANDLE;
};

/** @brief blur / composite 阶段的 push constants */
export struct BlurPushConstants {
    float texelOffsetX, texelOffsetY, radius, _pad;
    float u0, v0, u1, v1;
    float viewportW, viewportH;
};
static_assert(sizeof(BlurPushConstants) == 40, "BlurPushConstants size mismatch");

export struct QuadPushConstants {
    float posX, posY, sizeX, sizeY;                  // 元素逻辑框（未外扩）
    float cornerRadius, refraction, specular, _pad0;
    float m00, m01, m02, m10, m11, m12;
    float viewportW, viewportH;                      // = canvas extent（纹理与画布同构，UV=world/viewport）
};
static_assert(sizeof(QuadPushConstants) == 64, "QuadPushConstants size mismatch");

/**
 * @brief 液态玻璃 backdrop 后端渲染器
 *
 * 帧中途由 VulkanBackend::backdropBlur 调用：
 *   1) draw()      — 中断主 pass 后：canvas → capture(降采样) → blurH → blurV；
 *                    目标懒分配，renderpass 独立离屏（仅 color，无 stencil）。
 *   2) composite() — 主 pass 恢复后：以元素 rect(+t) 绘制，UV 映射捕获盒采样 pingB（模糊）
 *                    与 capture（原始），圆角 SDF 蒙版 + 可选边缘折射/高光。
 * 所有 pass 复用共享 vertexBuffer_/indexBuffer_（6 索引），与 ImageRenderer 同款。
 */
export class BackdropRenderer {
public:
    BackdropRenderer() = default;
    ~BackdropRenderer();

    bool create(VkDevice device, VkPipelineCache cache, VkPhysicalDevice phys, VkRenderPass mainPass, VkBuffer vertexBuffer,
                VkBuffer indexBuffer);
    void destroy();

    /** 帧内拦截：捕获 + 离屏模糊（canvas 须处于 TRANSFER_SRC 且主 pass 已结束）。
     *  目标固定为 canvas/4 尺寸（换画布才重建，帧内零重分配）；cap 为本元素捕获盒
     *  （已 ∩ 画布/scissor，物理 px），blit 到纹理内对应子区域；sigmaScreen 为屏幕空间高斯 σ */
    bool draw(VkCommandBuffer cb, VkExtent2D ctx, VkImage canvas, std::uint32_t canvasW, std::uint32_t canvasH,
              VkPhysicalDevice phys, const Rect &cap, float sigmaScreen);

    /** 主 pass 恢复后：合成到元素 rect（UV=世界坐标/视口，无需 cap） */
    void composite(VkCommandBuffer cb, VkExtent2D ctx, const BackdropBlurCmd &cmd, bool clipped);

private:
    void destroyTargets();
    bool ensureTargets(VkPhysicalDevice phys, std::uint32_t w, std::uint32_t h);
    VkRenderPass createOffscreenPass(VkFormat fmt);
    std::uint32_t downsample_ = 4;

    VkDevice device_ = VK_NULL_HANDLE;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;

    VkRenderPass offscreenPass_ = VK_NULL_HANDLE;
    VkFormat offscreenFormat_ = VK_FORMAT_R8G8B8A8_UNORM;

    BackdropTarget capture_;
    BackdropTarget pingA_;
    BackdropTarget pingB_;
    VkFramebuffer captureFbo_ = VK_NULL_HANDLE;
    VkFramebuffer pingAFbo_ = VK_NULL_HANDLE;
    VkFramebuffer pingBFbo_ = VK_NULL_HANDLE;
    std::uint32_t tw_ = 0, th_ = 0;

    VkDescriptorPool descPool_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descSetLayout_ = VK_NULL_HANDLE;

    VkPipelineLayout blurLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout quadLayout_ = VK_NULL_HANDLE;
    VkPipeline blurHPipeline_ = VK_NULL_HANDLE;
    VkPipeline blurVPipeline_ = VK_NULL_HANDLE;
    VkPipeline compositePipeline_ = VK_NULL_HANDLE;
    VkPipeline compositeClipPipeline_ = VK_NULL_HANDLE;
};