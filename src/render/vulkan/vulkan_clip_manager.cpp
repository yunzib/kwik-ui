module;
#include <vulkan/vulkan.h>   // ← VkRect2D, vkCmdSetScissor
#include <cstdint>            // ← int32_t, uint32_t
#include <cmath>  

module kwik.render.vulkan.clip_manager;
import kwik.render.vulkan.context;
import kwik.core.types;
import std;
void ClipManager::beginFrame() { clipStack_.clear(); clipSaveStack_.clear(); alphaSaveStack_.clear(); }
void ClipManager::pushClipRoundedRect(VulkanContext &ctx, const Rect &rect, float /*radius*/) {
    clipStack_.push_back(rect);
    VkRect2D sc = {{std::max(0, (int32_t)std::round(rect.x)), std::max(0, (int32_t)std::round(rect.y))},
                    {std::max(0u, (uint32_t)std::round(rect.width)), std::max(0u, (uint32_t)std::round(rect.height))}};
    vkCmdSetScissor(ctx.commandBuffer(), 0, 1, &sc);
}
void ClipManager::resetClip(VulkanContext &ctx) {
    if (!clipStack_.empty()) clipStack_.pop_back();
    VkRect2D sc;
    if (clipStack_.empty()) sc = {{0, 0}, ctx.extent()};
    else {
        auto &r = clipStack_.back();
        sc = {{(int32_t)std::max(0.f, std::round(r.x)), (int32_t)std::max(0.f, std::round(r.y))},
              {std::max(0u, (uint32_t)std::round(r.width)), std::max(0u, (uint32_t)std::round(r.height))}};
    }
    vkCmdSetScissor(ctx.commandBuffer(), 0, 1, &sc);
}
void ClipManager::saveState()   { clipSaveStack_.push_back(clipStack_); alphaSaveStack_.push_back(globalAlpha_); }
void ClipManager::restoreState(VulkanContext &ctx) {
    if (!clipSaveStack_.empty()) { clipStack_ = std::move(clipSaveStack_.back()); clipSaveStack_.pop_back(); }
    if (!alphaSaveStack_.empty()) { globalAlpha_ = alphaSaveStack_.back(); alphaSaveStack_.pop_back(); }
    VkRect2D sc;
    if (clipStack_.empty()) sc = {{0, 0}, ctx.extent()};
    else {
        auto &r = clipStack_.back();
        sc = {{(int32_t)std::max(0.f, std::round(r.x)), (int32_t)std::max(0.f, std::round(r.y))},
              {std::max(0u, (uint32_t)std::round(r.width)), std::max(0u, (uint32_t)std::round(r.height))}};
    }
    vkCmdSetScissor(ctx.commandBuffer(), 0, 1, &sc);
}
void ClipManager::setGlobalAlpha(float a) { globalAlpha_ = std::clamp(a, 0.0f, 1.0f); }