module;
#include <vector>
export module kwik.render.vulkan.clip_manager;
import kwik.core.types;
import kwik.render.vulkan.context;
/**
 * @brief 裁剪状态管理器 — scissor 矩形栈 + save/restore + alpha
 *
 * 当前版本使用 axis-aligned scissor 实现矩形裁剪。
 * 圆角裁剪暂不可实现，将来可通过 stencil subpass 扩展。
 */
export class ClipManager {
public:
    ClipManager() = default;
    void beginFrame();
    void pushClipRoundedRect(VulkanContext &ctx, const Rect &rect, float radius);
    void resetClip(VulkanContext &ctx);
    void saveState();
    void restoreState(VulkanContext &ctx);
    void setGlobalAlpha(float alpha);
    float globalAlpha() const {
        return globalAlpha_;
    }

    // ── Stencil 层级追踪 ────────────────────────────────
    /// 返回当前裁剪栈深度 (0 = 无裁剪)
    size_t level() const {
        return clipStack_.size();
    }

private:
    std::vector<Rect> clipStack_;
    std::vector<std::vector<Rect>> clipSaveStack_;
    float globalAlpha_ = 1.0f;
    std::vector<float> alphaSaveStack_;
};