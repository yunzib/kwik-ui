module;
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
export module kwik.render.vulkan_backend;
import kwik.render.backend;
import kwik.render.command;
import kwik.render.vulkan.context;
import kwik.render.vulkan.rect_renderer;
import kwik.render.vulkan.glyph_renderer;
import kwik.render.vulkan.image_renderer;
import kwik.render.vulkan.clip_manager;
import kwik.render.text.pipeline;
import kwik.render.text.types;

import kwik.core.types;

import std;

/**
 * @brief Vulkan 渲染后端聚合层
 *
 * 组合 VulkanContext + 3 个 Renderer + ClipManager，
 * 实现 RenderBackend 接口。自身不含任何渲染逻辑，纯委托。
 */
export class VulkanBackend : public RenderBackend {
public:
    VulkanBackend() = default;
    ~VulkanBackend() override;
    bool initialize(void *nativeHandle) override;
    void shutdown() override;
    bool resize(int width, int height) override;    // 返回 bool
    bool beginFrame(const Rect &dirtyRect) override;
    void endFrame() override;
    void present() override;
    // 形状
    void clear(const Color &color) override;
    void fillRect(const Rect &rect, const Color &color) override;
    void fillRoundedRect(const Rect &rect, float radius, const Color &color) override;
    void strokeRoundedRect(const Rect &rect, float radius, const Color &color, float strokeWidth) override;
    void drawShadow(const Rect &rect, float radius, const Shadow &shadow) override;
    // 文字
    void drawGlyph(const DrawGlyphCmd &cmd) override;

    // 图片
    void drawImage(const DrawImageCmd &cmd) override;
    uint32_t createImageTexture(const uint8_t *rgba, uint32_t width, uint32_t height) override;
    void destroyImageTexture(uint32_t id) override;
    // 裁剪
    void pushClipRoundedRect(const Rect &rect, float radius) override;
    void resetClip() override;
    void saveState() override;
    void restoreState() override;
    void setGlobalAlpha(float alpha) override;
    BackendType getType() const override {
        return BackendType::Vulkan;
    }
    int getWidth() const override {
        return width_;
    }
    int getHeight() const override {
        return height_;
    }


private:
    VulkanContext ctx_;
    RectRenderer rect_;
    GlyphRenderer glyph_;
    ImageRenderer image_;
    ClipManager clip_;
    std::optional<FrameToken> currentToken_;
    DeviceContext deviceCtx_;
    int width_ = 0;
    int height_ = 0;
};