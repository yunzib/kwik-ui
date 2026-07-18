module;

export module kwik.render.scene_builder;

import kwik.core.types;
import kwik.render.backend;
import kwik.render.draw_list;

import std;

/**
 * @brief 场景构建器 — 渲染线程 DFS 遍历层树时调用
 *
 * 在 RenderThread 中创建，遍历 Layer 树时将每个节点翻译为
 * 后端的 push/pop/drawPicture 调用。
 * 后端维护状态栈，实现变换/裁剪/透明度的嵌套。
 */
export class SceneBuilder {
public:
    /**
     * @brief 构造
     * @param backend 渲染后端引用
     */
    explicit SceneBuilder(RenderBackend &backend) : backend_(backend) {}

    /**
     * @brief 推入变换
     *
     * 后端将当前变换矩阵与新增变换复合。
     */
    void pushTransform(float tx, float ty, float sx, float sy) {
        backend_.pushTransform(tx, ty, sx, sy);
    }

    /**
     * @brief 推入圆角矩形裁剪
     */
    void pushClipRRect(const Rect &rect, float radius) {
        backend_.pushClipRoundedRect(rect, radius);
    }

    /**
     * @brief 推入透明度
     */
    void pushOpacity(float opacity) {
        backend_.setGlobalAlpha(opacity);
    }

    /**
     * @brief 绘制已录制的 Picture
     *
     * Picture 中的命令是局部坐标，后端变换栈已包含父级变换。
     */
    void drawList(const DrawList & drawList) {
        drawList.replay(backend_);
    }

    /**
     * @brief 弹出最近一次 push 的状态
     *
     * 后端恢复上一个状态（变换矩阵、裁剪、透明度）。
     */
    void pop() {
        backend_.popState();
    }

private:
    RenderBackend &backend_;  ///< 渲染后端引用
};