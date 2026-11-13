/** @brief G2D 元素: JS 工厂 / 方法 / 插件式注册 (自 bindings.cpp / element_parser.cpp 抽取)。 */
module;
// C printf 族桥声明（GCC16 std 模块边界，见头注释）
#include <kwik/c_bridge.h>
#include "quickjs.h"
// memcpy/strcmp 族 std 模块不导出（using 声明不可导出），文本 string.h 提供全局名（无 C++ 包装，与 std 模块无冲突）
#include <string.h>

module kwik.bridge.g2d;

import kwik.element.g2d;           // G2D 类
import kwik.core.types;            // Gradient / GradientType
import kwik.element.view;          // View (findById / unique_ptr<View>)
import kwik.element.typed_prop;    // TypedPropMap
import kwik.bridge.props_parser;   // PropsExtractor / parseViewProps
import kwik.bridge.bindings;       // makeElementHelper
import kwik.bridge.element_spec;   // ElementSpec
import kwik.bridge.element_parser; // ElementParser::registerExtension / applyBindings
import kwik.engine.context;        // QuickJSContext (getUserPointer)
import kwik.core.color_parser;     // parseColor (setFillStyle / setStrokeStyle)
import kwik.engine.js_value;
import kwik.event;                 // TargetLiveness（缓存指针存活校验）

import std;

// ── 辅助宏：提取 G2D* 指针 (自 bindings.cpp:1041 原样搬迁) ──
#define G2D_FROM_THIS(ctx, this_val)                                                                                   \
    [&]() -> G2D * {                                                                                                   \
        JSValue _p = JS_GetPropertyStr(ctx, this_val, "props");                                                        \
        /* 优先读 __g2d_ptr（eager 创建 or 首次 findById 后缓存）；缓存须经存活         \
           登记校验——树销毁（条件渲染删节点/reconcile 类型切换）后此处是悬空           \
           地址，直接使用即 UAF */                                                                                     \
        JSValue _ptr = JS_GetPropertyStr(ctx, _p, "__g2d_ptr");                                                        \
        if (!JS_IsUndefined(_ptr)) {                                                                                   \
            double _v;                                                                                                 \
            JS_ToFloat64(ctx, &_v, _ptr);                                                                              \
            auto *_cached = reinterpret_cast<G2D *>(static_cast<std::uintptr_t>(_v));                                       \
            if (TargetLiveness::instance().isAlive(_cached)) {                                                         \
                JS_FreeValue(ctx, _ptr);                                                                               \
                JS_FreeValue(ctx, _p);                                                                                 \
                return _cached;                                                                                        \
            }                                                                                                          \
            JS_SetPropertyStr(ctx, _p, "__g2d_ptr", JS_UNDEFINED);    /* 悬空缓存清除，走重解析 */                     \
        }                                                                                                              \
        JS_FreeValue(ctx, _ptr);                                                                                       \
        /* Fallback: findById（兼容树已建好但没有缓存的场景；树重建后同 id 命中新实例并回填缓存） */                   \
        JSValue _id = JS_GetPropertyStr(ctx, _p, "id");                                                                \
        const char *_idStr = JS_ToCString(ctx, _id);                                                                   \
        auto *_qctx = static_cast<QuickJSContext *>(JS_GetContextOpaque(ctx));                                         \
        View *_root = static_cast<View *>(_qctx->getUserPointer());                                                    \
        auto *_r = dynamic_cast<G2D *>(_root ? _root->findById(_idStr) : nullptr);                                     \
        if (_r) { /* 缓存到 __g2d_ptr 下次直接走快路径 */                                                              \
            JS_SetPropertyStr(ctx, _p, "__g2d_ptr",                                                                    \
                              JS_NewFloat64(ctx, static_cast<double>(reinterpret_cast<std::uintptr_t>(_r))));               \
        }                                                                                                              \
        JS_FreeCString(ctx, _idStr);                                                                                   \
        JS_FreeValue(ctx, _id);                                                                                        \
        JS_FreeValue(ctx, _p);                                                                                         \
        return _r;                                                                                                     \
    }()

// ── 数值参数提取 ──
// 此前各回调内联 lambda 不查 JS_ToFloat64 返回值与参数个数：非数值参数
// 得 NaN 坐标直进绘制命令（污染 path/矩形），失败挂起的 JS 异常被吞、
// 延迟到下一个不相关的 JS 调用错位冒出。统一走此处：数量不足/非有限
// 数值即抛 TypeError 中止本次绘制调用。
static std::optional<float> g2dNumArg(JSContext *ctx, int argc, JSValueConst *argv, int i) {
    if (i >= argc) {
        JS_ThrowTypeError(ctx, "g2d: 缺少第 %d 个数值参数", i + 1);
        return std::nullopt;
    }
    double v;
    if (JS_ToFloat64(ctx, &v, argv[i]) != 0 || !std::isfinite(v)) {
        JS_ThrowTypeError(ctx, "g2d: 第 %d 个参数不是有限数值", i + 1);
        return std::nullopt;
    }
    return static_cast<float>(v);
}

/// 提取 n 个数值参数到局部数组 _a（任一非法即 return JS_EXCEPTION，
/// 不残留 NaN 坐标或半截路径）
#define G2D_NUM_ARGS(n)                                                                                          \
    float _a[(n)];                                                                                               \
    do {                                                                                                         \
        for (int _i = 0; _i < (n); ++_i) {                                                                       \
            auto _v = g2dNumArg(ctx, argc, argv, _i);                                                            \
            if (!_v) { return JS_EXCEPTION; }                                                                    \
            _a[_i] = *_v;                                                                                        \
        }                                                                                                        \
    } while (0)

// ── 各方法回调 ──
static JSValue js_g2d_fillRect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    G2D_NUM_ARGS(4);
    g2d->fillRect(_a[0], _a[1], _a[2], _a[3]);
    return JS_UNDEFINED;
}

static JSValue js_g2d_strokeRect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    G2D_NUM_ARGS(4);
    g2d->strokeRect(_a[0], _a[1], _a[2], _a[3]);
    return JS_UNDEFINED;
}

static JSValue js_g2d_clearRect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    G2D_NUM_ARGS(4);
    g2d->clearRect(_a[0], _a[1], _a[2], _a[3]);
    return JS_UNDEFINED;
}

static JSValue js_g2d_beginPath(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    g2d->beginPath();
    return JS_UNDEFINED;
}

static JSValue js_g2d_moveTo(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    G2D_NUM_ARGS(2);
    g2d->moveTo(_a[0], _a[1]);
    return JS_UNDEFINED;
}

static JSValue js_g2d_lineTo(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    G2D_NUM_ARGS(2);
    g2d->lineTo(_a[0], _a[1]);
    return JS_UNDEFINED;
}

static JSValue js_g2d_quadraticCurveTo(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    G2D_NUM_ARGS(4);
    g2d->quadraticCurveTo(_a[0], _a[1], _a[2], _a[3]);
    return JS_UNDEFINED;
}

static JSValue js_g2d_bezierCurveTo(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    G2D_NUM_ARGS(6);
    g2d->bezierCurveTo(_a[0], _a[1], _a[2], _a[3], _a[4], _a[5]);
    return JS_UNDEFINED;
}

static JSValue js_g2d_arc(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    G2D_NUM_ARGS(5);
    bool ccw = argc > 5 ? JS_ToBool(ctx, argv[5]) != 0 : false;
    g2d->arc(_a[0], _a[1], _a[2], _a[3], _a[4], ccw);
    return JS_UNDEFINED;
}

static JSValue js_g2d_ellipse(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    G2D_NUM_ARGS(7);
    bool ccw = argc > 7 ? JS_ToBool(ctx, argv[7]) != 0 : false;
    g2d->ellipse(_a[0], _a[1], _a[2], _a[3], _a[4], _a[5], _a[6], ccw);
    return JS_UNDEFINED;
}

static JSValue js_g2d_closePath(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    g2d->closePath();
    return JS_UNDEFINED;
}

static JSValue js_g2d_fill(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    g2d->fill();
    return JS_UNDEFINED;
}

static JSValue js_g2d_stroke(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    g2d->stroke();
    return JS_UNDEFINED;
}

static JSValue js_g2d_clip(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    g2d->clip();
    return JS_UNDEFINED;
}

static JSValue js_g2d_save(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    g2d->save();
    return JS_UNDEFINED;
}

static JSValue js_g2d_restore(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    g2d->restore();
    return JS_UNDEFINED;
}

static JSValue js_g2d_drawImage(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    auto texId = g2dNumArg(ctx, argc, argv, 0);    // texId 也走有限数值校验
    if (!texId) return JS_EXCEPTION;
    float _a[4];
    for (int _i = 1; _i <= 4; ++_i) {              // 矩形 4 参（下标 1..4）
        auto v = g2dNumArg(ctx, argc, argv, _i);
        if (!v) { return JS_EXCEPTION; }
        _a[_i - 1] = *v;
    }
    g2d->drawImage(std::uint32_t(*texId), _a[0], _a[1], _a[2], _a[3]);
    return JS_UNDEFINED;
}

// ── 渐变对象 → Gradient（JS 侧 {__g2d_grad:true,type,x0..y1,c0,c1}）──
static bool g2dGradFromJS(JSContext *ctx, JSValueConst v, Gradient &out) {
    JSValue tag = JS_GetPropertyStr(ctx, v, "__g2d_grad");
    bool isGrad = JS_ToBool(ctx, tag) != 0;
    JS_FreeValue(ctx, tag);
    if (!isGrad) return false;
    JSValue type = JS_GetPropertyStr(ctx, v, "type");
    const char *ts = JS_ToCString(ctx, type);
    out.type = (ts && strcmp(ts, "radial") == 0) ? GradientType::Radial : GradientType::Linear;
    JS_FreeCString(ctx, ts);
    JS_FreeValue(ctx, type);
    auto num = [&](const char *k, float &dst) {
        JSValue p = JS_GetPropertyStr(ctx, v, k);
        double d = 0;
        JS_ToFloat64(ctx, &d, p);
        dst = static_cast<float>(d);
        JS_FreeValue(ctx, p);
    };
    num("x0", out.x0);
    num("y0", out.y0);
    num("x1", out.x1);
    num("y1", out.y1);
    auto col = [&](const char *k, Color &dst) {
        JSValue p = JS_GetPropertyStr(ctx, v, k);
        const char *s = JS_ToCString(ctx, p);
        if (s) dst = parseColor(s);
        JS_FreeCString(ctx, s);
        JS_FreeValue(ctx, p);
    };
    col("c0", out.color0);
    col("c1", out.color1);
    return true;
}

// ── 渐变工厂：createLinearGradient(x0,y0,x1,y1,c0,c1) / createRadialGradient(x0,y0,r,c0,c1) ──
static JSValue g2dMakeGradient(JSContext *ctx, const char *type, float x0, float y0, float x1, float y1,
                               const char *c0s, const char *c1s) {
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "__g2d_grad", JS_TRUE);
    JS_SetPropertyStr(ctx, o, "type", JS_NewString(ctx, type));
    JS_SetPropertyStr(ctx, o, "x0", JS_NewFloat64(ctx, x0));
    JS_SetPropertyStr(ctx, o, "y0", JS_NewFloat64(ctx, y0));
    JS_SetPropertyStr(ctx, o, "x1", JS_NewFloat64(ctx, x1));
    JS_SetPropertyStr(ctx, o, "y1", JS_NewFloat64(ctx, y1));
    JS_SetPropertyStr(ctx, o, "c0", JS_NewString(ctx, c0s ? c0s : "#000000"));
    JS_SetPropertyStr(ctx, o, "c1", JS_NewString(ctx, c1s ? c1s : "#000000"));
    return o;
}

static JSValue js_g2d_createLinearGradient(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    G2D_NUM_ARGS(4);
    const char *c0 = argc > 4 ? JS_ToCString(ctx, argv[4]) : nullptr;
    const char *c1 = argc > 5 ? JS_ToCString(ctx, argv[5]) : nullptr;
    JSValue o = g2dMakeGradient(ctx, "linear", _a[0], _a[1], _a[2], _a[3], c0, c1);
    if (c0) JS_FreeCString(ctx, c0);
    if (c1) JS_FreeCString(ctx, c1);
    return o;
}

static JSValue js_g2d_createRadialGradient(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    G2D_NUM_ARGS(3);
    const char *c0 = argc > 3 ? JS_ToCString(ctx, argv[3]) : nullptr;
    const char *c1 = argc > 4 ? JS_ToCString(ctx, argv[4]) : nullptr;
    JSValue o = g2dMakeGradient(ctx, "radial", _a[0], _a[1], _a[2], 0, c0, c1);
    if (c0) JS_FreeCString(ctx, c0);
    if (c1) JS_FreeCString(ctx, c1);
    return o;
}

static JSValue js_g2d_setFillStyle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    Gradient grad;
    if (argc > 0 && !JS_IsString(argv[0]) && g2dGradFromJS(ctx, argv[0], grad)) {
        g2d->setFillGradient(grad);    // 渐变画刷
        return JS_UNDEFINED;
    }
    const char *s = JS_ToCString(ctx, argv[0]);
    if (s) {
        g2d->setFillStyle(parseColor(s));
        JS_FreeCString(ctx, s);
    }
    return JS_UNDEFINED;
}

static JSValue js_g2d_setStrokeStyle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    Gradient grad;
    if (argc > 0 && !JS_IsString(argv[0]) && g2dGradFromJS(ctx, argv[0], grad)) {
        g2d->setStrokeGradient(grad);
        return JS_UNDEFINED;
    }
    const char *s = JS_ToCString(ctx, argv[0]);
    if (s) {
        g2d->setStrokeStyle(parseColor(s));
        JS_FreeCString(ctx, s);
    }
    return JS_UNDEFINED;
}

// ── P0 新增：fillText / translate / rotate / scale ──
static JSValue js_g2d_fillText(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    const char *s = JS_ToCString(ctx, argv[0]);
    if (!s) return JS_EXCEPTION;
    std::string text = s;
    JS_FreeCString(ctx, s);
    auto x = g2dNumArg(ctx, argc, argv, 1);
    auto y = g2dNumArg(ctx, argc, argv, 2);
    if (!x || !y) return JS_EXCEPTION;
    float fs = 14.0f;
    if (argc > 3) {
        auto v = g2dNumArg(ctx, argc, argv, 3);
        if (!v) return JS_EXCEPTION;
        fs = *v;
    }
    g2d->fillText(text, *x, *y, fs);
    return JS_UNDEFINED;
}

static JSValue js_g2d_translate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    G2D_NUM_ARGS(2);
    g2d->translate(_a[0], _a[1]);
    return JS_UNDEFINED;
}

static JSValue js_g2d_rotate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    G2D_NUM_ARGS(1);
    g2d->rotate(_a[0]);
    return JS_UNDEFINED;
}

static JSValue js_g2d_scale(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    G2D_NUM_ARGS(2);
    g2d->scale(_a[0], _a[1]);
    return JS_UNDEFINED;
}

static JSValue js_g2d_setLineWidth(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    double v;
    JS_ToFloat64(ctx, &v, argv[0]);
    g2d->setLineWidth(float(v));
    return JS_UNDEFINED;
}

static JSValue js_g2d_setGlobalAlpha(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    double v;
    JS_ToFloat64(ctx, &v, argv[0]);
    g2d->setGlobalAlpha(float(v));
    return JS_UNDEFINED;
}

static JSValue js_g2d_reset(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    g2d->reset();
    return JS_UNDEFINED;
}

// ── bindG2DMethods (自 bindings.cpp:1291 原样搬迁) ──
static void bindG2DMethods(JSContext *ctx, JSValue obj) {
    struct {
        const char *name;
        JSCFunction *fn;
        int argc;
    } methods[] = {
        {"fillRect", js_g2d_fillRect, 4},
        {"strokeRect", js_g2d_strokeRect, 4},
        {"clearRect", js_g2d_clearRect, 4},
        {"beginPath", js_g2d_beginPath, 0},
        {"moveTo", js_g2d_moveTo, 2},
        {"lineTo", js_g2d_lineTo, 2},
        {"quadraticCurveTo", js_g2d_quadraticCurveTo, 4},
        {"bezierCurveTo", js_g2d_bezierCurveTo, 6},
        {"arc", js_g2d_arc, 6},
        {"ellipse", js_g2d_ellipse, 8},
        {"closePath", js_g2d_closePath, 0},
        {"fill", js_g2d_fill, 0},
        {"stroke", js_g2d_stroke, 0},
        {"clip", js_g2d_clip, 0},
        {"save", js_g2d_save, 0},
        {"restore", js_g2d_restore, 0},
        {"drawImage", js_g2d_drawImage, 5},
        {"fillText", js_g2d_fillText, 4},
        {"translate", js_g2d_translate, 2},
        {"rotate", js_g2d_rotate, 1},
        {"scale", js_g2d_scale, 2},
        {"createLinearGradient", js_g2d_createLinearGradient, 6},
        {"createRadialGradient", js_g2d_createRadialGradient, 5},
        {"reset", js_g2d_reset, 0},
        {"setFillStyle", js_g2d_setFillStyle, 1},
        {"setStrokeStyle", js_g2d_setStrokeStyle, 1},
        {"setLineWidth", js_g2d_setLineWidth, 1},
        {"setGlobalAlpha", js_g2d_setGlobalAlpha, 1},
    };
    for (auto &m : methods) { JS_SetPropertyStr(ctx, obj, m.name, JS_NewCFunction(ctx, m.fn, m.name, m.argc)); }
}

// ── JS 工厂 (自 bindings.cpp:1323 搬迁, 仅 makeElement → makeElementHelper) ──
static JSValue js_g2d(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    // 非对象/缺参时新建 props（归本函数所有，用毕释放）：原实现把 JS_UNDEFINED
    // 传下去——id/__g2d_ptr 落在 undefined 上 SetProperty 留 pending exception，
    // 且 eager G2D 永不被树认领（泄漏）
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? JS_DupValue(ctx, argv[0]) : JS_NewObject(ctx);
    JSValue idVal = JS_GetPropertyStr(ctx, props, "id");
    if (JS_IsUndefined(idVal)) {
        static std::atomic<std::uint32_t> g2d_id_counter{0};
        char buf[64];
        kwik_snprintf(buf, sizeof(buf), "__g2d_%u", g2d_id_counter.fetch_add(1));
        JS_SetPropertyStr(ctx, props, "id", JS_NewString(ctx, buf));
    }
    JS_FreeValue(ctx, idVal);
    JSValue obj = makeElementHelper(ctx, "G2D", props, JS_UNDEFINED);
    if (!JS_IsException(obj)) {
        bindG2DMethods(ctx, obj);

        // 立即创建 C++ G2D, 指针存到 props (命令式绘制在树建立前就要到达 C++)
        auto *g2d = new G2D();
        JS_SetPropertyStr(ctx, props, "__g2d_ptr",
                          JS_NewFloat64(ctx, static_cast<double>(reinterpret_cast<std::uintptr_t>(g2d))));
    }
    JS_FreeValue(ctx, props);
    return obj;
}

// ── 创建器 (自 element_parser.cpp:444-465 搬迁) ──
static std::unique_ptr<View> createG2D(const JSValueRef &pv) {
    // 复用 eager 创建的 C++ G2D
    if (pv.hasProperty("__g2d_ptr")) {
        auto ptrVal = pv.getProperty("__g2d_ptr");
        JSContext *ctx = pv.context();
        double v;
        JS_ToFloat64(ctx, &v, ptrVal.raw());
        auto *existing = reinterpret_cast<G2D *>(static_cast<std::uintptr_t>(v));
        TypedPropMap meta;
        PropsExtractor ex(pv, &meta);
        existing->props = parseViewProps(ex);     // 更新 ViewProps (width/height 等)
        return std::unique_ptr<G2D>(existing);    // 树接管所有权
    }
    // 降级: 无 eager 场景正常创建
    TypedPropMap meta;
    PropsExtractor ex(pv, &meta);
    auto v = std::make_unique<G2D>(parseViewProps(ex));
    v->propMeta = std::move(meta);
    applyBindings(v.get(), pv);
    return v;
}

// ── 自注册 ──
void registerG2DElement() {
    ElementSpec spec;
    spec.typeName = "G2D";
    spec.creator = createG2D;
    // reconcileProps / attachHandlers 无需: G2D 无专有 reconcile / 事件形状
    spec.jsFactoryName = "G2D";
    spec.jsFactoryFn = js_g2d;
    spec.jsFactoryArgc = 1;
    ElementParser::registerExtension(std::move(spec));
}