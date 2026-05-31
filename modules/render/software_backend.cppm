module;
#include <cstdint>

export module kwik.render.software_backend;

import kwik.render.backend;
import kwik.core.types;
import kwik.render.command;
import std;

/**
 * @brief 软件渲染后端实现（CPU渲染）
 */
export class SoftwareBackend : public RenderBackend {
public:
    SoftwareBackend();
    SoftwareBackend(int width, int height);
    ~SoftwareBackend() override;

    bool initialize(void *nativeHandle, int width, int height) override;
    void shutdown() override;
    void resize(int width, int height) override;
    // bool beginFrame() override;
    void endFrame() override;
    void present() override;
    void setGlobalAlpha(float alpha) override;
    void pushClipRoundedRect(const Rect &rect, float radius) override;
    void resetClip() override;
    void clear(const Color &color) override;
    void fillRect(const Rect &rect, const Color &color) override;
    void fillRoundedRect(const Rect &rect, float radius, const Color &color) override;
    void strokeRoundedRect(const Rect &rect, float radius, const Color &color, float strokeWidth) override;
    void drawShadow(const Rect &rect, float radius, const Shadow &shadow) override;
    void drawGlyph(const DrawGlyphCmd &cmd) override;
    void uploadGlyphAtlas(const uint8_t *data, uint32_t width, uint32_t height) override;
    BackendType getType() const override {
        return BackendType::Software;
    }
    int getWidth() const override {
        return width_;
    }
    int getHeight() const override {
        return height_;
    }

private:
    int width_ = 0;
    int height_ = 0;
    float globalAlpha_ = 1.0f;
    // 像素缓冲区
    std::vector<uint32_t> pixelBuffer_;
    // 裁剪栈等
};
