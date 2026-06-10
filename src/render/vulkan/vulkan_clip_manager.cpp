module;
#include <vulkan/vulkan.h>
#include <cstdint>
#include <cmath>
#include <vector>

module kwik.render.vulkan.clip_manager;
import kwik.core.types;
import std;

void ClipManager::beginFrame(const VkExtent2D &, const VkRect2D &initialScissor) {
    clipStack_.clear();
    clipSaveStack_.clear();
    alphaSaveStack_.clear();
    initialScissor_ = initialScissor;
}
void ClipManager::pushClipRoundedRect(VkCommandBuffer cmd, const Rect &rect, float /*radius*/) {
    clipStack_.push_back(rect);
    VkRect2D sc = {{std::max(0, (int32_t)std::round(rect.x)), std::max(0, (int32_t)std::round(rect.y))},
                   {std::max(0u, (uint32_t)std::round(rect.width)), std::max(0u, (uint32_t)std::round(rect.height))}};
    vkCmdSetScissor(cmd, 0, 1, &sc);
}
void ClipManager::resetClip(VkCommandBuffer cmd) {
    if (!clipStack_.empty()) clipStack_.pop_back();
    VkRect2D sc;
    if (clipStack_.empty())
        sc = initialScissor_;
    else {
        auto &r = clipStack_.back();
        sc = {{(int32_t)std::max(0.f, std::round(r.x)), (int32_t)std::max(0.f, std::round(r.y))},
              {std::max(0u, (uint32_t)std::round(r.width)), std::max(0u, (uint32_t)std::round(r.height))}};
    }
    vkCmdSetScissor(cmd, 0, 1, &sc);
}
void ClipManager::saveState() {
    clipSaveStack_.push_back(clipStack_);
    alphaSaveStack_.push_back(globalAlpha_);
}
void ClipManager::restoreState(VkCommandBuffer cmd) {
    if (!clipSaveStack_.empty()) {
        clipStack_ = std::move(clipSaveStack_.back());
        clipSaveStack_.pop_back();
    }
    if (!alphaSaveStack_.empty()) {
        globalAlpha_ = alphaSaveStack_.back();
        alphaSaveStack_.pop_back();
    }
    VkRect2D sc;
    if (clipStack_.empty())
        sc = initialScissor_;
    else {
        auto &r = clipStack_.back();
        sc = {{(int32_t)std::max(0.f, std::round(r.x)), (int32_t)std::max(0.f, std::round(r.y))},
              {std::max(0u, (uint32_t)std::round(r.width)), std::max(0u, (uint32_t)std::round(r.height))}};
    }
    vkCmdSetScissor(cmd, 0, 1, &sc);
}
void ClipManager::setGlobalAlpha(float a) {
    globalAlpha_ = std::clamp(a, 0.0f, 1.0f);
}