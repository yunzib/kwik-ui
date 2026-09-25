module;

#include <cstddef>
#include <functional>
#include <vector>

export module kwik.element.layer_stack;

import kwik.core.types;
import kwik.element.view;
import kwik.event;
import kwik.render.graphics;

import std;

/**
 * @brief LayerStack — 层树管理（多图层：base 树 + 有序弹层）
 *
 * S2-2 自 layer_view 模块拆出为独立模块：View::layers() 上行访问要求
 * view.cppm 能引用本类型（layer_view import view，反向即循环依赖）。
 *
 * 多呈现（清单 §十四）：每棵 UI 树一份（KwikRuntime 成员），经 View 基类
 * 服务槽接线（根节点 setTreeService），组件经 View::layers() 上行访问。
 *
 * 职责（只管绘制顺序 + 事件路由根；事件/模态语义留 widget 级）：
 *   1. 持有 base 视图树根 + 有序 LayerView 列表（borrowed 非拥有指针）
 *   2. drawAll(): base 绘制 → 逐层底→顶绘制
 *   3. hitTest(): 顶→底遍历 layers 再 base（事件路由根）
 *   4. 作为 EventTarget 挂到 EventRouter（setRootTarget）
 */
export class LayerStack : public EventTarget {
public:
    LayerStack() = default;

    // ── base 树管理（KwikRuntime 在 init/rebuild/HMR 后注入）──
    void setBase(View *base) { base_ = base; }
    View *base() const { return base_; }

    // ── 图层注册（LayerView/Keyboard/Dropdown/DateTimePicker 调用）──
    /** 注册图层（底→顶顺序，后注册在上层 = z 序） */
    void registerLayerView(View *layer);
    /** 注销图层 */
    void unregisterLayerView(View *layer);
    /** 清空图层列表（HMR tree_.reset() 前调用，防 borrowed 指针悬空） */
    void clear();
    /** 当前图层数 */
    size_t layerCount() const { return layers_.size(); }
    /** 底→顶遍历各图层（清单接线：KwikRuntime 组装复合根清单用） */
    void forEachLayer(const std::function<void(View *)> &fn) {
        for (auto *l : layers_) fn(l);
    }

    // ── 帧绘制入口 ──
    /**
     * @brief 统一绘制所有层
     * @param g           图形上下文（与单树模式共用，复用层树缓存）
     * @param dirtyAccum  脏矩形累加器（保留参数，历史兼容）
     */
    void drawAll(Graphics &g, Rect *dirtyAccum);

    // ── EventTarget 实现（事件路由根）──
    EventTarget *hitTest(Point point) override;
    bool onEvent(const DispatchEvent &event) override { return false; }
    EventTarget *parent() const override { return nullptr; }

private:
    View *base_ = nullptr;                // base 视图树根（非拥有；实为 RootView*，按 View 接口使用）
    std::vector<View *> layers_;          // 有序图层（底→顶，borrowed 非拥有）
};

/** @brief 取组件所在树的层树（类型安全收口：void* 槽唯一 cast 点）。
 *  组件在树内（parent 链有效）时使用；根未接线返回 nullptr（防御） */
export inline LayerStack *layersOf(const View *v) {
    return static_cast<LayerStack *>(v->treeService(View::kSvcLayerStack));
}
