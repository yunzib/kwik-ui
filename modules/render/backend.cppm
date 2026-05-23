module;
#include <cstdint>

export module kwik.render.backend;

import kwik.core.types;
import kwik.render.command;
import kwik.platform.window;
import std;

/**
 * @brief 渲染后端类型枚举
 */
export enum class BackendType {
    Software, // CPU软件渲染
    Vulkan,   // Vulkan图形API
    OpenGL,   // OpenGL图形API
    Metal     // Metal图形API (macOS/iOS)
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
     * @param width 初始宽度
     * @param height 初始高度
     * @return 初始化是否成功
     */
    virtual bool initialize(void *nativeHandle, int width, int height) = 0;

    /**
     * @brief 清理后端资源
     */
    virtual void shutdown() = 0;

    /**
     * @brief 调整渲染尺寸
     */
    virtual void resize(int width, int height) = 0;

    /**
     * @brief 开始一帧渲染
     * @return 如果帧缓冲区准备就绪返回true
     */
    virtual bool beginFrame() = 0;

    /**
     * @brief 结束一帧渲染
     */
    virtual void endFrame() = 0;

    /**
     * @brief 呈现当前帧到窗口
     */
    virtual void present() = 0;

    virtual void drawGlyph(const DrawGlyphCmd &cmd) = 0;
    virtual void uploadGlyphAtlas(const uint8_t *data, uint32_t width, uint32_t height) = 0;

    /**
     * @brief 设置全局透明度（0.0 - 1.0）
     */
    virtual void setGlobalAlpha(float alpha) = 0;

    /**
     * @brief 推送圆角矩形裁剪区域
     */
    virtual void pushClipRoundedRect(const Rect &rect, float radius) = 0;

    /**
     * @brief 重置裁剪区域
     */
    virtual void resetClip() = 0;

    /**
     * @brief 清空画布
     */
    virtual void clear(const Color &color) = 0;

    /**
     * @brief 填充矩形
     */
    virtual void fillRect(const Rect &rect, const Color &color) = 0;

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

    virtual void saveClipState() = 0;
    virtual void restoreClipState() = 0;

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
