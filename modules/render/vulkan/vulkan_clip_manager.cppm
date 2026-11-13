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
export module kwik.render.vulkan.clip_manager;
import std;
import kwik.core.types;

export class ClipManager {
public:
    ClipManager() = default;
    void beginFrame(const VkExtent2D &, const VkRect2D &initialScissor);
    void pushClipRoundedRect(VkCommandBuffer cmd, const Rect &rect, float radius);
    void resetClip(VkCommandBuffer cmd);
    void saveState();
    void restoreState(VkCommandBuffer cmd);
    void setGlobalAlpha(float alpha);
    float globalAlpha() const {
        return globalAlpha_;
    }
    std::size_t level() const {
        return clipStack_.size();
    }

    /** @brief 当前最内侧裁剪矩形（物理），无裁剪返回 nullopt */
    std::optional<Rect> currentScissor() const;

    /** @brief 当前可画域（物理）：裁剪栈顶；空栈回退伤害带 scissor
     *  （beginFrame 已对渲染区求交）——mesh 等自带视口命令的求交基准 */
    Rect effectiveBounds() const;

    /** @brief 与当前可画域求交（空交集收成 0×0 合法空剪） */
    Rect intersectCurrent(const Rect &r) const;

    /** @brief 任意矩形收口成合法 scissor（对渲染区求交后转换；越界矩形
     *  直接作 vkCmdSetScissor 按 Vulkan 规范是无效操作） */
    VkRect2D toScissor(const Rect &r) const;

    /** @brief 无栈扰动地把 2D 默认动态态重新应用到 GPU：scissor（栈顶或
     *  伤害带）+ viewport 回全屏 + 模板状态（render pass 重启后恢复、
     *  mesh/backdrop 等自带视口命令收尾用） */
    void reapplyState(VkCommandBuffer cmd);

private:
    std::vector<Rect> clipStack_;
    std::vector<std::vector<Rect>> clipSaveStack_;
    float globalAlpha_ = 1.0f;
    std::vector<float> alphaSaveStack_;
    VkRect2D initialScissor_ = {{0, 0}, {0, 0}};
    VkExtent2D renderExtent_ = {0, 0};    // 渲染区上界（beginFrame 记录，scissor 收口求交基准）
};