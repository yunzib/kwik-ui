module;
#include <cstdint>

export module kwik.render.backend;

import kwik.core.types;
import kwik.render.command;
import kwik.platform.window;
import kwik.core.path; // Vec2 定义在此模块中

import std;

/**
 * @brief 渲染后端类型枚举
 */
export enum class BackendType {
    Software,    // CPU软件渲染
    Vulkan,      // Vulkan图形API
    OpenGL,      // OpenGL图形API
    Metal        // Metal图形API (macOS/iOS)
};

/**
 * @brief 渲染后端抽象基类
 *
 * 定义不同图形API后端的统一接口，支持运行时切换
 */
export class RenderBackend {
public:
    virtual ~RenderBackend() = default;

    virtual bool initialize(void *nativeHandle) = 0;
    virtual void shutdown() = 0;
    virtual bool resize(int width, int height) = 0;

    /**
     * @brief 开始一帧渲染
     * @param dirtyRect 脏区域 (物理像素坐标), 空=全屏
     */
    virtual bool beginFrame(const Rect &dirtyRect) = 0;

    /** @brief 结束一帧渲染 */
    virtual void endFrame() = 0;

    /** @brief 呈现当前帧到窗口 */
    virtual bool present() = 0;

    virtual void drawGlyph(const DrawGlyphCmd &cmd) = 0;    // cmd 内含 Transform2D t

    /** @brief 清空画布 */
    virtual void clear(const Color &color) = 0;

    /**
     * @brief 填充矩形
     * @param rect 矩形区域（逻辑坐标）
     * @param color 填充颜色
     * @param mode  混合模式（SrcOver / SrcCopy）
     * @param t     2D 变换矩阵（GPU 端变换）
     */
    virtual void fillRect(const Rect &rect, const Color &color, BlendMode mode, const Transform2D &t) = 0;

    /** @brief 填充圆角矩形（逻辑坐标 + 矩阵） */
    virtual void fillRoundedRect(const Rect &rect, float radius, const Color &color, const Gradient &gradient,
                                 const Transform2D &t) = 0;

    /** @brief 线段胶囊描边（cmd 内含矩阵） */
    virtual void drawSegment(const DrawSegmentCmd &cmd) = 0;

    /** @brief 描边圆角矩形（逻辑坐标 + 矩阵） */
    virtual void strokeRoundedRect(const Rect &rect, float radius, const Color &color, float strokeWidth,
                                   const Transform2D &t) = 0;

    /** @brief 绘制阴影（逻辑坐标 + 矩阵） */
    virtual void drawShadow(const Rect &rect, float radius, const Shadow &shadow, const Transform2D &t) = 0;

    /** @brief 绘制图像（cmd 内含矩阵） */
    virtual void drawImage(const DrawImageCmd &cmd) = 0;

    /** @brief 填充三角形网格（cmd 内含矩阵） */
    virtual void fillTriangles(const FillTrianglesCmd &cmd, const AAVertex *vertices) = 0;

    /** @brief 绘制 3D 网格（对象空间 MVP，无 2D 矩阵） */
    virtual void drawMesh(const DrawMeshCmd &cmd, const Vertex3D *vertices) = 0;

    virtual uint32_t createImageTexture(const uint8_t *rgba, uint32_t width, uint32_t height) = 0;
    virtual void destroyImageTexture(uint32_t id) = 0;

    /**
     * @brief 推入圆角矩形裁剪（局部坐标 + 矩阵，stencil 掩码旋转裁剪；clipRect 为物理 AABB 供 scissor）
     */
    virtual void pushClipRoundedRect(const Rect &rect, float radius, const Transform2D &t, const Rect &clipRect) = 0;

    /**
     * @brief 弹出最近一次 clip（PopClipCmd 回放时调用）
     */
    virtual void popState() = 0;

    virtual BackendType getType() const = 0;
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
};