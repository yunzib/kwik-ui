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
    bool present() override;
    // 形状
    void clear(const Color &color) override;
    /**
     * @brief 填充矩形
     * @param rect  矩形区域
     * @param color 填充颜色
     * @param mode  混合模式：SrcOver（默认）走 rect renderer，SrcCopy 走 vkCmdClearAttachments
     */
    void fillRect(const Rect &rect, const Color &color, BlendMode mode) override;
    void fillRoundedRect(const Rect &rect, float radius, const Color &color) override;
     void drawSegment(const DrawSegmentCmd &cmd) override;
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

    void popState() override;
    BackendType getType() const override { return BackendType::Vulkan; }
    int getWidth() const override { return width_; }
    int getHeight() const override { return height_; }

    void fillTriangles(const FillTrianglesCmd &cmd, const AAVertex *vertices) override;
    void drawMesh(const DrawMeshCmd &cmd, const Vertex3D *vertices) override;

    /**
     * @brief 重置帧内 GPU 状态缓存（结构变化时调用）
     *
     * 当前为 no-op：Vulkan 后端无帧级状态缓存。
     * 保留接口供未来添加 dirty rect 追踪、管线缓存等优化。
     */
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
    TriangleRenderer triangle_;    ///< 三角形网格渲染器
    MeshRenderer mesh_;            ///< 3D 网格渲染器 (深度测试)

    // 状态栈：变换矩阵、透明度
    struct State {
        float tx = 0, ty = 0;
        float sx = 1, sy = 1;
        float alpha = 1.0f;
    };
    std::vector<State> stateStack_;
    State currentState_;

    enum class PushKind : uint8_t { Transform, Clip, Alpha };
    std::vector<PushKind> pushKinds_;

    uint32_t drawCalls_ = 0;   ///< 临时探针：本帧绘制调用计数
};