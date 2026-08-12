// ============================================================================
// js_lazy_list_source.cppm — JS 虚拟化列表数据源
//
// 把 JS items 数组 + itemBuilder 函数包装为 LazyListSource 接口实现。
// element 层只看抽象接口，QuickJS 细节全部隔离在本模块。
// ============================================================================

module;
#include "quickjs.h"

export module kwik.bridge.js_lazy_list_source;

import kwik.element.lazy_list_source;
import kwik.engine.js_value;
import kwik.element.view;

import std;

/**
 * @brief 基于 JS items 数组 + itemBuilder 函数的列表数据源
 *
 * 持有 items 与 itemBuilder 的 dup 引用（析构释放）。
 * buildItem：items[index] + index → JS_Call(itemBuilder) → parseNode 成 C++ View。
 * discardItem：递归解绑整棵 item 子树的 BindingRegistry（bindings 逐 View 注册，
 *              registry 的 unbind(View*) 只删单节点，须递归走子树）。
 */
export class JsLazyListSource : public LazyListSource {
public:
    /**
     * @brief 构造
     * @param ctx         QuickJS 上下文
     * @param items       items 数组（内部 dup，析构释放）
     * @param itemBuilder itemBuilder 函数（内部 dup，析构释放；可为 undefined → 空行占位）
     */
    JsLazyListSource(JSContext *ctx, JSValue items, JSValue itemBuilder);
    ~JsLazyListSource() override;

    // 禁止拷贝（JSValue 引用不可浅拷贝）
    JsLazyListSource(const JsLazyListSource &) = delete;
    JsLazyListSource &operator=(const JsLazyListSource &) = delete;

    // ── LazyListSource 接口 ──
    int itemCount() const override;
    std::unique_ptr<View> buildItem(int index) override;
    void discardItem(int index, View *item) override;

private:
    JSContext *ctx_ = nullptr;
    JSValue items_{JS_UNDEFINED};      // items 数组（dup 持有）
    JSValue builder_{JS_UNDEFINED};    // itemBuilder 函数（dup 持有）
};

/**
 * @brief 创建 JS 虚拟化列表数据源（工厂，供 element_parser 经钩子注入）
 * @return LazyListSource 智能指针；items 非法时仍返回实例（itemCount=0 → 空列表）
 */
export std::unique_ptr<LazyListSource> createJsLazyListSource(JSContext *ctx, JSValue items, JSValue itemBuilder);