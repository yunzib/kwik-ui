// ============================================================================
// lazy_list_source.cppm — LazyList 虚拟化数据源抽象接口
//
// 引擎中立接口：让 LazyList 完全脱离对 JS 引擎的依赖。
// 具体实现（JsLazyListSource）在 bridge 层 kwik.bridge.js_lazy_list_source 完成，
// 生命周期由 LazyList 以 unique_ptr 持有（~LazyList 内先逐个 discardItem 再析构）。
// ============================================================================

export module kwik.element.lazy_list_source;

import kwik.element.view;

import std;

export class LazyListSource {
public:
    virtual ~LazyListSource() = default;

    /// 数据行总数（无数据/非法返回 0）
    virtual int itemCount() const = 0;

    /**
     * @brief 构建第 index 行（内部：itemBuilder(index) → parseNode）
     * @param index 数据行索引（0 起）
     * @return 行节点（所有权转移）；失败时实现应返回空 View 占位，保证索引对齐
     */
    virtual std::unique_ptr<View> buildItem(int index) = 0;

    /**
     * @brief 出窗/销毁前回调（bridge 实现在此解绑整棵 item 子树的 BindingRegistry，
     *        防 State 悬空指针 UAF——bindings 是逐 View 注册的，须递归解绑）
     */
    virtual void discardItem(int index, View *item) = 0;
};