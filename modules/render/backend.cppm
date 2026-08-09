module;
#include <cstdint>

export module kwik.render.backend;

import kwik.core.types;
import kwik.render.command;
import kwik.platform.window;
import kwik.core.path;   // Vec2 定义在此模块中

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

    /**
     * @brief 初始化后端
     * @param nativeHandle 原生窗口句柄
     * @return 初始化是否成功
     */
    virtual bool initialize(void *nativeHandle) = 0;

    /**
     * @brief 清理后端资源
     */
    virtual void shutdown() = 0;

    /**
     * @brief 调整渲染尺寸
     */
    virtual bool resize(int width, int height) = 0;

    /**
     * @brief 开始一帧渲染
     * @param dirtyRect 脏区域 (物理像素坐标), 空=全屏
     * @return 帧缓冲区就绪返回 true
     */
    virtual bool beginFrame(const Rect &dirtyRect) = 0;

    /**
     * @brief 结束一帧渲染
     */
    virtual void endFrame() = 0;

    /**
     * @brief 呈现当前帧到窗口
     */
    virtual bool present() = 0;

    virtual void drawGlyph(const DrawGlyphCmd &cmd) = 0;

    /**
     * @brief 清空画布
     */
    virtual void clear(const Color &color) = 0;

    /**
     * @brief 填充矩形
     * @param rect 矩形区域（屏幕坐标）
     * @param color 填充颜色
     * @param mode  混合模式（默认 SrcOver 标准混合，SrcCopy 用于清除）
     */
    virtual void fillRect(const Rect &rect, const Color &color, BlendMode mode = BlendMode::SrcOver) = 0;

    /**
     * @brief 填充圆角矩形
     */
    virtual void fillRoundedRect(const Rect &rect, float radius, const Color &color) = 0;

    /**
     * @brief 描边圆角矩形
     */
    virtual void strokeRoundedRect(const Rect &rect, float radius, const Color &color, float strokeWidth) = 0;

    /**
     * @brief 绘制阴影
     */
    virtual void drawShadow(const Rect &rect, float radius, const Shadow &shadow) = 0;

    /**
     * @brief 绘制图像
     * @param cmd 包含纹理句柄、目标矩形、透明度
     */
    virtual void drawImage(const DrawImageCmd &cmd) = 0;

    /**
     * @brief 填充三角形网格
     * @param cmd 命令元数据（offset + count + color）
     * @param vertices 顶点数据指针
     */
    virtual void fillTriangles(const FillTrianglesCmd &cmd, const Vec2 *vertices) = 0;

    /**
     * @brief 绘制 3D 网格（深度测试）
     * @param cmd      命令元数据（offset + count + mvp + color + lightDir）
     * @param vertices Vertex3D 顶点数据指针（位置 + 法线）
     */
    virtual void drawMesh(const DrawMeshCmd &cmd, const Vertex3D *vertices) = 0;

    /**
     * @brief 创建图像纹理并上传 RGBA 像素到 GPU
     * @param rgba    RGBA8 像素数据 (4 bytes per pixel)
     * @param width   图像宽度 (像素)
     * @param height  图像高度 (像素)
     * @return 非零纹理句柄 (0 表示失败)
     *
     * 返回的句柄用于 drawImage() 和 destroyImageTexture()。
     * 创建后 CPU 端数据可安全释放。
     */
    virtual uint32_t createImageTexture(const uint8_t *rgba, uint32_t width, uint32_t height) = 0;
    /**
     * @brief 销毁图像纹理并释放 GPU 资源
     * @param id createImageTexture() 返回的句柄
     */
    virtual void destroyImageTexture(uint32_t id) = 0;


    // ═══════════════════════════════════════════════
    // 新增：层树遍历接口
    // ═══════════════════════════════════════════════

    /**
     * @brief 推入变换矩阵
     *
     * 将当前变换矩阵与 (tx,ty) 平移和 (sx,sy) 缩放复合。
     * 变换通过 push constant 或 uniform 在 GPU 端应用。
     */
    virtual void pushTransform(float tx, float ty, float sx, float sy) = 0;

    /**
     * @brief 推入圆角矩形裁剪
     *
     * 与当前裁剪区域求交，通过 stencil buffer 实现圆角。
     */
    virtual void pushClipRoundedRect(const Rect &rect, float radius) = 0;

    /**
     * @brief 设置全局透明度
     *
     * 后续绘制命令的颜色 alpha 乘以此值。
     * 与 pushTransform/pushClipRoundedRect 一样受 popState 管理。
     */
    virtual void setGlobalAlpha(float alpha) = 0;

    /**
     * @brief 弹出最近一次 push 的状态
     *
     * 恢复上一个变换矩阵、裁剪区域和透明度。
     * 后端内部维护状态栈。
     */
    virtual void popState() = 0;


    /**
     * @brief 获取后端类型
     */
    virtual BackendType getType() const = 0;

    /**
     * @brief 获取当前宽度
     */
    virtual int getWidth() const = 0;

    /**
     * @brief 获取当前高度
     */
    virtual int getHeight() const = 0;
};
