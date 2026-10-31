module;
#include <vulkan/vulkan.h>
#include <optional>
#include <vector>
export module kwik.render.vulkan.clip_manager;
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
    size_t level() const {
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