module;
#include <vulkan/vulkan.h>
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

private:
    std::vector<Rect> clipStack_;
    std::vector<std::vector<Rect>> clipSaveStack_;
    float globalAlpha_ = 1.0f;
    std::vector<float> alphaSaveStack_;
    VkRect2D initialScissor_ = {{0, 0}, {0, 0}};
};