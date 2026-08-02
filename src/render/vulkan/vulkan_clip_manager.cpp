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
    // 负尺寸必须先在 float 域 clamp 到 0 再转 uint32;
    // 原实现先转 uint32 → 负值(-64)变巨大正数(4294967232) → scissor 溢出
    VkRect2D sc = {
        {std::max(0, (int32_t)std::round(rect.x)), std::max(0, (int32_t)std::round(rect.y))},
        {(uint32_t)std::max(0.0f, std::round(rect.width)), (uint32_t)std::max(0.0f, std::round(rect.height))}};
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
              {(uint32_t)std::max(0.0f, std::round(r.width)), (uint32_t)std::max(0.0f, std::round(r.height))}};
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
              {(uint32_t)std::max(0.0f, std::round(r.width)), (uint32_t)std::max(0.0f, std::round(r.height))}};
    }
    vkCmdSetScissor(cmd, 0, 1, &sc);
}
void ClipManager::setGlobalAlpha(float a) {
    globalAlpha_ = std::clamp(a, 0.0f, 1.0f);
}