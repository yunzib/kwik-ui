module;

#include <cstdint>
#include <memory>
#include <vector>

export module kwik.render.layer;

import kwik.core.types;
import kwik.render.draw_list;
import kwik.render.scene_builder;

import std;

/**
 * @brief 层树基类
 *
 * 每个 Layer 代表一个渲染树节点。
 * 层树跨帧保留，属性变化时只更新局部节点。
 */
export class Layer {
public:
    virtual ~Layer() = default;

    /**
     * @brief DFS 遍历：将本层提交到 SceneBuilder
     * @param sb 场景构建器（渲染线程使用）
     */
    virtual void visit(SceneBuilder &sb) const = 0;

    /**
     * @brief 标记本层需要更新（属性变化时调用）
     */
    void markDirty() { dirty_ = true; }

    /**
     * @brief 清除脏标记（visit 后调用）
     */
    void markClean() { dirty_ = false; }

    /**
     * @brief 本层是否需要重新提交到 GPU
     */
    bool isDirty() const { return dirty_; }

    /**
     * @brief 获取本层包围盒（用于脏区域合并）
     */
    Rect bounds() const { return bounds_; }

    /**
     * @brief 设置本层包围盒
     */
    void setBounds(const Rect &r) { bounds_ = r; }

protected:
    bool dirty_ = true;   ///< 初始为脏，首帧全量提交
    Rect bounds_ = {};    ///< 局部坐标包围盒
};

/**
 * @brief 容器层基类 — 持有子层列表
 */
export class ContainerLayer : public Layer {
public:
    /**
     * @brief 添加子层
     * @param child 子层所有权
     */
    void addChild(std::unique_ptr<Layer> child) {
        children_.push_back(std::move(child));
    }

    /**
     * @brief 移除所有子层（结构变化时调用）
     */
    void removeAllChildren() { children_.clear(); }

    void visit(SceneBuilder &sb) const override;

    /**
     * @brief 获取子层列表（只读）
     */
    const std::vector<std::unique_ptr<Layer>> &children() const { return children_; }

protected:
    std::vector<std::unique_ptr<Layer>> children_;  ///< 子层列表（有序）
};

/**
 * @brief 变换层 — 对子层应用平移+缩放
 *
 * 变换由 GPU 通过 push constant 处理，不烘烤到子层坐标中。
 */
export class TransformLayer : public ContainerLayer {
public:
    TransformLayer(float tx, float ty, float sx, float sy)
        : tx_(tx), ty_(ty), sx_(sx), sy_(sy) {}

    /**
     * @brief 更新变换参数（属性动画时调用）
     */
    void setTransform(float tx, float ty, float sx, float sy) {
        tx_ = tx; ty_ = ty; sx_ = sx; sy_ = sy;
        markDirty();
    }

    void visit(SceneBuilder &sb) const override;

private:
    float tx_, ty_;  ///< 平移量
    float sx_, sy_;  ///< 缩放因子
};

/**
 * @brief 裁剪层 — 对子层应用圆角矩形裁剪
 */
export class ClipRRectLayer : public ContainerLayer {
public:
    ClipRRectLayer(const Rect &rect, float radius)
        : rect_(rect), radius_(radius) {}

    void visit(SceneBuilder &sb) const override;

private:
    Rect rect_;
    float radius_;
};

/**
 * @brief 透明度层 — 对子层应用统一透明度
 */
export class OpacityLayer : public ContainerLayer {
public:
    explicit OpacityLayer(float opacity) : opacity_(opacity) {}

    void setOpacity(float opacity) {
        opacity_ = opacity;
        markDirty();
    }

    void visit(SceneBuilder &sb) const override;

private:
    float opacity_;
};

/**
 * @brief 图片层 — 包含一个已录制的绘制命令集合
 *
 * DrawList 不可变，可跨帧共享。
 * 同一 DrawList 可在不同帧复用（通过 shared_ptr）。
 */
export class DrawListLayer : public Layer {
public:
    explicit DrawListLayer(std::shared_ptr<DrawList> drawList)
        : drawList_(std::move(drawList)) {}

    /**
     * @brief 替换图片（重绘时调用）
     */
    void setDrawList(std::shared_ptr<DrawList> drawList) {
        drawList_ = std::move(drawList);
        markDirty();
    }

    /** @brief 获取图片指针（渲染线程读取） */
    const DrawList *drawList() const { return drawList_.get(); }

    void visit(SceneBuilder &sb) const override;

private:
    std::shared_ptr<DrawList> drawList_;  ///< 共享图片，跨帧复用
};