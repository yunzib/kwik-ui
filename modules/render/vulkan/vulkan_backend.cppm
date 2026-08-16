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
import kwik.render.vulkan.triangle_renderer;
import kwik.core.path;
import kwik.render.vulkan.mesh_renderer;
import kwik.core.types;

import std;

/**
 * @brief Vulkan 渲染后端聚合层
 *
 * 组合 VulkanContext + 各 Renderer + ClipManager，实现 RenderBackend 接口。
 * 自身不含渲染逻辑，纯委托（含矩阵透传）。
 */
export class VulkanBackend : public RenderBackend {
public:
    VulkanBackend() = default;
    ~VulkanBackend() override;
    bool initialize(void *nativeHandle) override;
    void shutdown() override;
    bool resize(int width, int height) override;
    bool beginFrame(const Rect &dirtyRect) override;
    void endFrame() override;
    bool present() override;

    void clear(const Color &color) override;
    void fillRect(const Rect &rect, const Color &color, BlendMode mode, const Transform2D &t) override;
    void fillRoundedRect(const Rect &rect, float radius, const Color &color, const Gradient &gradient,
                         const Transform2D &t) override;
    void drawSegment(const DrawSegmentCmd &cmd) override;
    void strokeRoundedRect(const Rect &rect, float radius, const Color &color, float strokeWidth,
                           const Transform2D &t) override;
    void drawShadow(const Rect &rect, float radius, const Shadow &shadow, const Transform2D &t) override;
    void drawGlyph(const DrawGlyphCmd &cmd) override;
    void drawImage(const DrawImageCmd &cmd) override;
    uint32_t createImageTexture(const uint8_t *rgba, uint32_t width, uint32_t height) override;
    void destroyImageTexture(uint32_t id) override;
    void pushClipRoundedRect(const Rect &rect, float radius, const Transform2D &t, const Rect &clipRect) override;
    void popState() override;
    BackendType getType() const override { return BackendType::Vulkan; }
    int getWidth() const override { return width_; }
    int getHeight() const override { return height_; }
    void fillTriangles(const FillTrianglesCmd &cmd, const AAVertex *vertices,
                       const SweepGrad *sweep = nullptr) override;
    void fillRing(const FillRingCmd &cmd) override;

    void drawMesh(const DrawMeshCmd &cmd, const Vertex3D *vertices) override;

    /** @brief 重置帧内 GPU 状态缓存（当前 no-op，保留接口） */
    void resetFrameCache() {}

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
    TriangleRenderer triangle_;
    MeshRenderer mesh_;

    enum class PushKind : uint8_t { Clip };    // 仅剩 Clip（transform/opacity 已烘烤）
    std::vector<PushKind> pushKinds_;

    uint32_t drawCalls_ = 0;    ///< 临时探针：本帧绘制调用计数
};