// ============================================================================
// event_adapter.cpp — JS 事件适配层实现
//
// 所有 lambda 统一捕获:
//   ctx — QuickJS 上下文 (JS_Call 用)
//   h   — shared_ptr<JSValueRef>, RAII 持有 JS 回调函数引用;
//         std::function 析构 → JSValueRef 析构 → JS_FreeValue,
//         与旧 ViewEventHandlers::release 的生命周期语义完全一致
//         (Application 先 tree_.reset() 后释放 JS 运行时)。
// ============================================================================

module;
#include "quickjs.h"
#include <cstdio>

module kwik.bridge.event_adapter;

import kwik.element.view;
import kwik.element.table;    // Table — onRowClick 数据源通道
import kwik.element.textview; // TextView::runs — 富文本事件负载
import kwik.engine.js_value;
import kwik.core.props; // TextRun, FontWeight, FontStyle
import kwik.core.types;
import kwik.core.input_session;    // InputMode — makeFocusEvent mode 字符串映射
import kwik.core.log;
import kwik.bridge.js_table_data_source; // JsTableDataSource — onRowClick 行对象回取

import std;

namespace {

/** @brief JS 回调函数的共享持有器 (lambda 捕获用, 析构自动 JS_FreeValue) */
using JsHandler = std::shared_ptr<JSValueRef>;

/**
 * @brief 统一调用 JS 回调并处理异常
 * @param ctx  QuickJS 上下文
 * @param h    JS 回调函数引用
 * @param arg  事件参数 (本函数接管并负责释放)
 * @param tag  日志标签 (事件名)
 * @return true=调用成功(消费), false=JS 异常
 */
bool callJs1(JSContext *ctx, const JsHandler &h, JSValue arg, const char *tag) {
    JSValue ret = JS_Call(ctx, h->raw(), JS_UNDEFINED, 1, &arg);
    JS_FreeValue(ctx, arg);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(ctx);
        const char *s = JS_ToCString(ctx, exc);
        Log::error("[{}] event callback error: {}", tag, s ? s : "unknown");
        JS_FreeCString(ctx, s);
        JS_FreeValue(ctx, exc);
        JS_FreeValue(ctx, ret);
        return false;
    }
    JS_FreeValue(ctx, ret);
    return true;
}

/** @brief callJs1 的无参版本（onClose/onMount/onUnmount 共用形态） */
void callJs0(JSContext *ctx, const JsHandler &h, const char *tag) {
    JSValue ret = JS_Call(ctx, h->raw(), JS_UNDEFINED, 0, nullptr);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(ctx);
        const char *s = JS_ToCString(ctx, exc);
        Log::error("[{}] event callback error: {}", tag, s ? s : "unknown");
        JS_FreeCString(ctx, s);
        JS_FreeValue(ctx, exc);
    }
    JS_FreeValue(ctx, ret);
}

/** @brief 构造指针事件对象 { x, y, gx, gy, dx, dy }（局部 + 窗口双载；
 *  dx/dy 为 pan 族相对按下点累计位移，其余事件恒 0） */
JSValue makePointerEvent(JSContext *ctx, const PointerArgs &a) {
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "x", JS_NewFloat64(ctx, static_cast<double>(a.x)));
    JS_SetPropertyStr(ctx, obj, "y", JS_NewFloat64(ctx, static_cast<double>(a.y)));
    JS_SetPropertyStr(ctx, obj, "gx", JS_NewFloat64(ctx, static_cast<double>(a.gx)));
    JS_SetPropertyStr(ctx, obj, "gy", JS_NewFloat64(ctx, static_cast<double>(a.gy)));
    JS_SetPropertyStr(ctx, obj, "dx", JS_NewFloat64(ctx, static_cast<double>(a.dx)));
    JS_SetPropertyStr(ctx, obj, "dy", JS_NewFloat64(ctx, static_cast<double>(a.dy)));
    return obj;
}

/**
 * @brief 构造 TextView 富文本事件 (runs 数组)
 *
 * 逻辑整体迁移自原 TextView::fireChange_, 字段逐一对齐:
 * [{ text, fontSize, fontWeight, fontStyle, underline, strikethrough, textColor }, ...]
 */
JSValue makeTextViewRunsEvent(JSContext *ctx, TextView &tv) {
    JSValue arr = JS_NewArray(ctx);
    const auto &runs = tv.runs();
    for (size_t i = 0; i < runs.size(); ++i) {
        auto &run = runs[i];
        JSValue obj = JS_NewObject(ctx);

        JS_SetPropertyStr(ctx, obj, "text", JS_NewString(ctx, run.text.c_str()));

        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f", run.style.fontSize);
        JS_SetPropertyStr(ctx, obj, "fontSize", JS_NewString(ctx, buf));

        JS_SetPropertyStr(ctx, obj, "fontWeight",
                          JS_NewString(ctx, run.style.fontWeight == FontWeight::Bold ? "bold" : "normal"));
        JS_SetPropertyStr(ctx, obj, "fontStyle",
                          JS_NewString(ctx, run.style.fontStyle == FontStyle::Italic ? "italic" : "normal"));
        JS_SetPropertyStr(ctx, obj, "underline", JS_NewBool(ctx, run.style.underline ? 1 : 0));
        JS_SetPropertyStr(ctx, obj, "strikethrough", JS_NewBool(ctx, run.style.strikethrough ? 1 : 0));

        auto cs =
            std::format("#{:02X}{:02X}{:02X}", run.style.textColor.r, run.style.textColor.g, run.style.textColor.b);
        JS_SetPropertyStr(ctx, obj, "textColor", JS_NewString(ctx, cs.c_str()));

        JS_SetPropertyUint32(ctx, arr, static_cast<uint32_t>(i), obj);
    }
    return arr;
}

/**
 * @brief 构造 onFocus / onBlur 的 JS 事件参数
 *
 * JS 契约: { mode: "text" | "number" }（输入模式=会话快照；非编辑器 Text）
 */
JSValue makeFocusEvent(JSContext *ctx, const FocusArgs &a) {
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "mode",
                      JS_NewString(ctx, a.mode == InputMode::Number ? "number" : "text"));
    return obj;
}

/**
 * @brief 按组件类型构造 onChange 的 JS 事件参数
 *
 * JS 契约 (必须与重构前行为逐字段一致):
 *   Input/TextArea              → 裸 string (非对象!)
 *   Checkbox/Switch/RadioButton → { checked: bool }
 *   Tabs/Dropdown               → { value: string, index: int }
 *   Slider                      → { value: number }
 *   RadioGroup                  → { value: string }
 *   TextView                    → runs 数组 (payload 仅作触发, 现场拉取)
 *   TreeMenu                    → { checked: string[], count: int }
 *   其余                        → 按 variant 实际类型兜底包装 { value: ... }
 */
JSValue makeChangeEvent(JSContext *ctx, View &view, const ChangeArgs &a) {
    switch (view.type()) {
    case ElementType::Input:
    case ElementType::TextArea: return JS_NewString(ctx, std::get<std::string>(a.value).c_str());

    case ElementType::Checkbox:
    case ElementType::Switch:
    case ElementType::RadioButton: {
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "checked", JS_NewBool(ctx, std::get<bool>(a.value) ? 1 : 0));
        return obj;
    }

    case ElementType::Tabs:
    case ElementType::Dropdown: {
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "value", JS_NewString(ctx, std::get<std::string>(a.value).c_str()));
        JS_SetPropertyStr(ctx, obj, "index", JS_NewInt32(ctx, a.index));
        return obj;
    }

    case ElementType::Slider: {
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "value", JS_NewFloat64(ctx, std::get<double>(a.value)));
        return obj;
    }

    case ElementType::RadioGroup: {
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "value", JS_NewString(ctx, std::get<std::string>(a.value).c_str()));
        return obj;
    }

    case ElementType::TextView: return makeTextViewRunsEvent(ctx, static_cast<TextView &>(view));

    case ElementType::StackIndex: {
        // JS 事件形状: { index: number } (无文本 value)
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "index", JS_NewInt32(ctx, a.index));
        return obj;
    }

    case ElementType::TreeMenu: {
        // a.value 为逗号连接的关键字串（TreeMenu::fireChange 拼接），此处切回数组
        std::string s = std::get<std::string>(a.value);
        JSValue arr = JS_NewArray(ctx);
        uint32_t n = 0;
        std::string cur;
        for (char ch : s) {
            if (ch == ',') {
                JS_SetPropertyUint32(ctx, arr, n++, JS_NewString(ctx, cur.c_str()));
                cur.clear();
            } else
                cur += ch;
        }
        if (!cur.empty()) JS_SetPropertyUint32(ctx, arr, n++, JS_NewString(ctx, cur.c_str()));
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "checked", arr);
        JS_SetPropertyStr(ctx, obj, "count", JS_NewInt32(ctx, a.index));
        return obj;
    }

    case ElementType::DateTimePicker: {
        // JS 事件形状: { value: "YYYY-MM-DD" | "HH:MM" | "YYYY-MM-DD HH:MM" }
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "value", JS_NewString(ctx, std::get<std::string>(a.value).c_str()));
        return obj;
    }

    case ElementType::SpinBox: {
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "value", JS_NewFloat64(ctx, std::get<double>(a.value)));
        return obj;
    }

    default: {
        JSValue obj = JS_NewObject(ctx);
        if (auto *s = std::get_if<std::string>(&a.value)) {
            JS_SetPropertyStr(ctx, obj, "value", JS_NewString(ctx, s->c_str()));
        } else if (auto *b = std::get_if<bool>(&a.value)) {
            JS_SetPropertyStr(ctx, obj, "value", JS_NewBool(ctx, *b ? 1 : 0));
        } else if (auto *d = std::get_if<double>(&a.value)) {
            JS_SetPropertyStr(ctx, obj, "value", JS_NewFloat64(ctx, *d));
        } else if (auto *i = std::get_if<std::int64_t>(&a.value)) {
            JS_SetPropertyStr(ctx, obj, "value", JS_NewInt32(ctx, static_cast<int32_t>(*i)));
        }
        return obj;
    }
    }
}

/**
 * @brief 从 props 读取函数属性并 Dup 为共享持有器
 * @return 属性不存在或非函数时返回 nullptr
 */
JsHandler dupHandler(JSContext *ctx, const JSValueRef &props, const char *name) {
    if (!props.hasProperty(name)) return nullptr;
    auto raw = props.getProperty(name);    // RAII, 作用域结束自动释放
    if (!JS_IsFunction(ctx, raw.raw())) return nullptr;
    // JSValueRef 构造即接管引用 → 先 Dup 再交给 RAII 管理
    return std::make_shared<JSValueRef>(ctx, JS_DupValue(ctx, raw.raw()));
}

}    // namespace

void attachJsHandlers(View &view, const JSValueRef &props) {
    if (!props.isObject()) return;
    JSContext *ctx = props.context();
    if (!ctx) return;

    // ── 指针事件: onClick / onLongPress / onHoverEnter / onHoverLeave ──
    auto bindPointer = [&](const char *name, std::function<bool(const PointerArgs &)> &slot) {
        if (auto h = dupHandler(ctx, props, name)) {
            slot = [ctx, h, name](const PointerArgs &a) { return callJs1(ctx, h, makePointerEvent(ctx, a), name); };
        }
    };
    bindPointer("onClick", view.handlers.onClick);
    bindPointer("onLongPress", view.handlers.onLongPress);
    bindPointer("onHoverEnter", view.handlers.onHoverEnter);
    bindPointer("onHoverLeave", view.handlers.onHoverLeave);
    bindPointer("onContextMenu", view.handlers.onContextMenu);
    // ── 拖动手势: onPanBegin / onPanMove / onPanEnd ──
    // 位移超 5px 阈值起拖；载荷 dx/dy = 相对按下点累计位移（PanBegin=0）；
    // 绑定即消费（阻止继续上溯）——自带拖拽的组件（Slider/滚动条/可选文本）
    // 自身消费 pan，槽位不触发
    bindPointer("onPanBegin", view.handlers.onPanBegin);
    bindPointer("onPanMove", view.handlers.onPanMove);
    bindPointer("onPanEnd", view.handlers.onPanEnd);

    // ── onWindowResize: 窗口尺寸变化（载荷 {width, height}）──
    // dispatch 以根为唯一预设目标——仅根声明生效；位置参数 x/y 即新宽/高
    if (auto h = dupHandler(ctx, props, "onWindowResize")) {
        view.handlers.onWindowResize = [ctx, h](const PointerArgs &a) {
            JSValue evt = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, evt, "width", JS_NewFloat64(ctx, a.x));
            JS_SetPropertyStr(ctx, evt, "height", JS_NewFloat64(ctx, a.y));
            return callJs1(ctx, h, evt, "onWindowResize");
        };
    }

    // ── onWindowStateChange: 窗口状态翻转（{state, maximized, minimized}）──
    // 载荷走位置参数 x=state（0 普通 / 1 最大化 / 2 最小化）；根直投
    if (auto h = dupHandler(ctx, props, "onWindowStateChange")) {
        view.handlers.onWindowStateChange = [ctx, h](const PointerArgs &a) {
            int state = (int)a.x;
            JSValue evt = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, evt, "state", JS_NewInt32(ctx, state));
            JS_SetPropertyStr(ctx, evt, "maximized", JS_NewBool(ctx, state == 1));
            JS_SetPropertyStr(ctx, evt, "minimized", JS_NewBool(ctx, state == 2));
            return callJs1(ctx, h, evt, "onWindowStateChange");
        };
    }

    // ── onChange: 捕获 View*, 调用时按组件类型现场构造事件对象 ──
    // (View* 与 std::function 同属于该 View, 同生共死, 无悬垂)
    if (auto h = dupHandler(ctx, props, "onChange")) {
        View *v = &view;
        view.handlers.onChange = [ctx, h, v](const ChangeArgs &a) {
            callJs1(ctx, h, makeChangeEvent(ctx, *v, a), "onChange");
        };
    }

    // ── onClose (Dialog, 无参) ──
    if (auto h = dupHandler(ctx, props, "onClose")) {
        view.handlers.onClose = [ctx, h]() {
            JSValue ret = JS_Call(ctx, h->raw(), JS_UNDEFINED, 0, nullptr);
            if (JS_IsException(ret)) {
                JSValue exc = JS_GetException(ctx);
                const char *s = JS_ToCString(ctx, exc);
                Log::error("[onClose] event callback error: {}", s ? s : "unknown");
                JS_FreeCString(ctx, s);
                JS_FreeValue(ctx, exc);
            }
            JS_FreeValue(ctx, ret);
        };
    }

    // ── onFocus / onBlur (焦点获得/失去, e={mode 输入模式}; FocusManager 注入直投) ──
    // 表单校验常用：Input/TextArea/TextView 等可聚焦组件声明即生效。
    // 载荷 mode 与会话快照同源（View::publishedTraits()）；旧无参形状升级
    if (auto h = dupHandler(ctx, props, "onFocus")) {
        view.handlers.onFocus = [ctx, h](const FocusArgs &a) { callJs1(ctx, h, makeFocusEvent(ctx, a), "onFocus"); };
    }
    if (auto h = dupHandler(ctx, props, "onBlur")) {
        view.handlers.onBlur = [ctx, h](const FocusArgs &a) { callJs1(ctx, h, makeFocusEvent(ctx, a), "onBlur"); };
    }

    // ── onEnter (Enter 提交, e={value 引擎处理后值}) ──
    // 仅 '\n' 输入触发（隐式失焦永不触发）；次序 onChange→onEnter→onBlur。
    // Input 提交值、TextArea 换行插入后全文——观察面批次
    if (auto h = dupHandler(ctx, props, "onEnter")) {
        view.handlers.onEnter = [ctx, h](const EnterArgs &a) {
            JSValue evt = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, evt, "value", JS_NewString(ctx, a.value.c_str()));
            return callJs1(ctx, h, evt, "onEnter");
        };
    }

    // ── onLinkClick (TextView 行内链接, e={url}) ──
    // readOnly 点击 / 编辑态 Ctrl+点击 落在 link run 上触发（编辑态普通
    // 点击仍走光标定位——编辑优先）。url 为 run.link 原样回传
    if (auto h = dupHandler(ctx, props, "onLinkClick")) {
        view.handlers.onLinkClick = [ctx, h](const LinkArgs &a) {
            JSValue evt = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, evt, "url", JS_NewString(ctx, a.url.c_str()));
            return callJs1(ctx, h, evt, "onLinkClick");
        };
    }

    // ── onError (资源失败上报, 参数 = 错误描述字符串; Image 装载/上传失败) ──
    if (auto h = dupHandler(ctx, props, "onError")) {
        view.handlers.onError = [ctx, h](std::string msg) {
            callJs1(ctx, h, JS_NewString(ctx, msg.c_str()), "onError");
        };
    }

    // ── onMount / onUnmount (通用生命周期, 无参) ──
    // onMount: 树构建完成后由 ElementParser::firePendingMounts 触发（仅新建节点）
    // onUnmount: reconcile 拆除路径在析构前触发；HMR 整树重建不触发
    // （JS 侧清理依赖 ctx 销毁自动回收）
    if (auto h = dupHandler(ctx, props, "onMount")) {
        view.handlers.onMount = [ctx, h]() { callJs0(ctx, h, "onMount"); };
    }
    if (auto h = dupHandler(ctx, props, "onUnmount")) {
        view.handlers.onUnmount = [ctx, h]() { callJs0(ctx, h, "onUnmount"); };
    }

    // ── onRowClick (Table): 行对象现场从数据源拉取 ──
    // 数据源具体类型 JsTableDataSource 为 bridge 内部实现,
    // 经 dynamic_cast 回取后构造 { index, row } (契约与历史一致)。
    // (View* 与 std::function 同属该 View, 同生共死, 无悬垂)
    if (view.type() == ElementType::Table) {
        if (auto h = dupHandler(ctx, props, "onRowClick")) {
            View *v = &view;
            view.handlers.onRowClick = [ctx, h, v](const RowArgs &a) {
                auto *jsData = dynamic_cast<JsTableDataSource *>(static_cast<Table *>(v)->dataSource());
                if (!jsData) return;
                JSValue evt = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, evt, "index", JS_NewInt32(ctx, a.index));
                // rowValueAt 返回 dup 引用, 由 JS_SetPropertyStr 消费
                JS_SetPropertyStr(ctx, evt, "row", jsData->rowValueAt(a.index));
                callJs1(ctx, h, evt, "onRowClick");
            };
        }
    }

    // ── onTreeExpand / onTreeSelect (TreeMenu): 单节点展开通报 / 单行点击通报 ──
    // 组件侧直派（toggleExpand 翻转后 / 行区 Tap 内），此处仅构造载荷：
    //   onTreeExpand e={key, expanded}（key 缺省回退 title；expanded=翻转后新态）
    //   onTreeSelect e={key}（单行点击身份通报；勾选框热区不发，走 onChange 族）
    if (view.type() == ElementType::TreeMenu) {
        if (auto h = dupHandler(ctx, props, "onTreeExpand")) {
            view.handlers.onTreeExpand = [ctx, h](const TreeArgs &a) {
                JSValue evt = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, evt, "key", JS_NewString(ctx, a.key.c_str()));
                JS_SetPropertyStr(ctx, evt, "expanded", JS_NewBool(ctx, a.expanded ? 1 : 0));
                callJs1(ctx, h, evt, "onTreeExpand");
            };
        }
        if (auto h = dupHandler(ctx, props, "onTreeSelect")) {
            view.handlers.onTreeSelect = [ctx, h](const TreeArgs &a) {
                JSValue evt = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, evt, "key", JS_NewString(ctx, a.key.c_str()));
                callJs1(ctx, h, evt, "onTreeSelect");
            };
        }
    }

    // ── onKey (Keyboard): 虚拟键盘按键旁路通知 ──
    // 只触发回调，不干预同路径自动注入
    if (view.type() == ElementType::Keyboard) {
        if (auto h = dupHandler(ctx, props, "onKey")) {
            view.handlers.onKey = [ctx, h](const KeyArgs &a) {
                JSValue evt = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, evt, "value", JS_NewString(ctx, a.value.c_str()));
                JS_SetPropertyStr(ctx, evt, "charCode", JS_NewUint32(ctx, a.charCode));
                JS_SetPropertyStr(ctx, evt, "keyCode", JS_NewUint32(ctx, a.keyCode));
                JS_SetPropertyStr(ctx, evt, "modifiers", JS_NewUint32(ctx, a.modifiers));    // bit1=Shift（0x2）等
                JSValue ret = JS_Call(ctx, h->raw(), JS_UNDEFINED, 1, &evt);
                JS_FreeValue(ctx, evt);
                if (JS_IsException(ret)) {
                    JSValue exc = JS_GetException(ctx);
                    const char *s = JS_ToCString(ctx, exc);
                    Log::error("[onKey] event callback error: {}", s ? s : "unknown");
                    JS_FreeCString(ctx, s);
                    JS_FreeValue(ctx, exc);
                }
                JS_FreeValue(ctx, ret);
            };
        }
    }
}