/** @brief G3D 扩展: JS 工厂 / 方法 / 插件式注册。 */
module;
#include "quickjs.h"
#include <cstdio>
#include <atomic>

module kwik.ext.g3d;

import kwik.element.view;          // View (findById / unique_ptr<View>)
import kwik.element.typed_prop;    // TypedPropMap
import kwik.bridge.props_parser;   // PropsExtractor / parseViewProps
import kwik.bridge.bindings;       // makeElementHelper
import kwik.bridge.element_spec;   // ElementSpec
import kwik.bridge.element_parser; // ElementParser::registerExtension / applyBindings
import kwik.engine.context;        // QuickJSContext (getUserPointer)
import kwik.core.color_parser;     // parseColor (setColor 用)
import kwik.engine.js_value;

import std;

// ── 辅助宏：提取 G3D* 指针 (自 bindings.cpp 原样搬迁) ──
#define G3D_FROM_THIS(ctx, this_val)                                                                                   \
    [&]() -> G3D * {                                                                                                   \
        JSValue _p = JS_GetPropertyStr(ctx, this_val, "props");                                                        \
        JSValue _ptr = JS_GetPropertyStr(ctx, _p, "__g3d_ptr");                                                        \
        if (!JS_IsUndefined(_ptr)) {                                                                                   \
            double _v;                                                                                                 \
            JS_ToFloat64(ctx, &_v, _ptr);                                                                              \
            JS_FreeValue(ctx, _ptr);                                                                                   \
            JS_FreeValue(ctx, _p);                                                                                     \
            return reinterpret_cast<G3D *>(static_cast<uintptr_t>(_v));                                                \
        }                                                                                                              \
        JS_FreeValue(ctx, _ptr);                                                                                       \
        JSValue _id = JS_GetPropertyStr(ctx, _p, "id");                                                                \
        const char *_idStr = JS_ToCString(ctx, _id);                                                                   \
        auto *_qctx = static_cast<QuickJSContext *>(JS_GetContextOpaque(ctx));                                         \
        View *_root = static_cast<View *>(_qctx->getUserPointer());                                                    \
        auto *_r = dynamic_cast<G3D *>(_root ? _root->findById(_idStr) : nullptr);                                     \
        if (_r) {                                                                                                      \
            JS_SetPropertyStr(ctx, _p, "__g3d_ptr",                                                                    \
                              JS_NewFloat64(ctx, static_cast<double>(reinterpret_cast<uintptr_t>(_r))));               \
        }                                                                                                              \
        JS_FreeCString(ctx, _idStr);                                                                                   \
        JS_FreeValue(ctx, _id);                                                                                        \
        JS_FreeValue(ctx, _p);                                                                                         \
        return _r;                                                                                                     \
    }()

// ── 9 个 js_g3d_* 方法回调 (自 bindings.cpp 原样搬迁) ──
static JSValue js_g3d_loadModel(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g3d = G3D_FROM_THIS(ctx, this_val);
    if (!g3d) return JS_UNDEFINED;
    const char *s = JS_ToCString(ctx, argv[0]);
    if (s) {
        g3d->loadModel(s);
        JS_FreeCString(ctx, s);
    }
    return JS_UNDEFINED;
}
static JSValue js_g3d_clear(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g3d = G3D_FROM_THIS(ctx, this_val);
    if (!g3d) return JS_UNDEFINED;
    g3d->clearModel();
    return JS_UNDEFINED;
}
static JSValue js_g3d_setColor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g3d = G3D_FROM_THIS(ctx, this_val);
    if (!g3d) return JS_UNDEFINED;
    const char *s = JS_ToCString(ctx, argv[0]);
    if (s) {
        g3d->setColor(parseColor(s));
        JS_FreeCString(ctx, s);
    }
    return JS_UNDEFINED;
}
static JSValue js_g3d_autoRotate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g3d = G3D_FROM_THIS(ctx, this_val);
    if (!g3d) return JS_UNDEFINED;
    g3d->setAutoRotate(JS_ToBool(ctx, argv[0]) != 0);
    return JS_UNDEFINED;
}
static JSValue js_g3d_showAxes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g3d = G3D_FROM_THIS(ctx, this_val);
    if (!g3d) return JS_UNDEFINED;
    g3d->setShowAxes(JS_ToBool(ctx, argv[0]) != 0);
    return JS_UNDEFINED;
}
static JSValue js_g3d_rotateTo(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g3d = G3D_FROM_THIS(ctx, this_val);
    if (!g3d) return JS_UNDEFINED;
    double yaw = 0, pitch = 0;
    JS_ToFloat64(ctx, &yaw, argv[0]);
    JS_ToFloat64(ctx, &pitch, argv[1]);
    g3d->rotateTo(float(yaw), float(pitch));
    return JS_UNDEFINED;
}
static JSValue js_g3d_addBox(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g3d = G3D_FROM_THIS(ctx, this_val);
    if (!g3d) return JS_UNDEFINED;
    double s = 1.0, tx = 0, ty = 0, tz = 0;
    JS_ToFloat64(ctx, &s, argv[0]);
    if (argc > 1) JS_ToFloat64(ctx, &tx, argv[1]);
    if (argc > 2) JS_ToFloat64(ctx, &ty, argv[2]);
    if (argc > 3) JS_ToFloat64(ctx, &tz, argv[3]);
    g3d->addBox(float(s), float(tx), float(ty), float(tz));
    return JS_UNDEFINED;
}
static JSValue js_g3d_addSphere(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g3d = G3D_FROM_THIS(ctx, this_val);
    if (!g3d) return JS_UNDEFINED;
    double r = 1.0, sl = 16, st = 12, tx = 0, ty = 0, tz = 0;
    JS_ToFloat64(ctx, &r, argv[0]);
    JS_ToFloat64(ctx, &sl, argv[1]);
    JS_ToFloat64(ctx, &st, argv[2]);
    if (argc > 3) JS_ToFloat64(ctx, &tx, argv[3]);
    if (argc > 4) JS_ToFloat64(ctx, &ty, argv[4]);
    if (argc > 5) JS_ToFloat64(ctx, &tz, argv[5]);
    g3d->addSphere(float(r), int(sl), int(st), float(tx), float(ty), float(tz));
    return JS_UNDEFINED;
}
static JSValue js_g3d_addPlane(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g3d = G3D_FROM_THIS(ctx, this_val);
    if (!g3d) return JS_UNDEFINED;
    double w = 4.0, d = 4.0, tx = 0, ty = 0, tz = 0;
    JS_ToFloat64(ctx, &w, argv[0]);
    JS_ToFloat64(ctx, &d, argv[1]);
    if (argc > 2) JS_ToFloat64(ctx, &tx, argv[2]);
    if (argc > 3) JS_ToFloat64(ctx, &ty, argv[3]);
    if (argc > 4) JS_ToFloat64(ctx, &tz, argv[4]);
    g3d->addPlane(float(w), float(d), float(tx), float(ty), float(tz));
    return JS_UNDEFINED;
}

// ── bindG3DMethods (原样搬迁, 含原重复 autoRotate 项) ──
static void bindG3DMethods(JSContext *ctx, JSValue obj) {
    struct {
        const char *name;
        JSCFunction *fn;
        int argc;
    } methods[] = {
        {"loadModel", js_g3d_loadModel, 1},   {"clear", js_g3d_clear, 0},       {"setColor", js_g3d_setColor, 1},
        {"autoRotate", js_g3d_autoRotate, 1}, {"rotateTo", js_g3d_rotateTo, 2}, {"addBox", js_g3d_addBox, 4},
        {"autoRotate", js_g3d_autoRotate, 1}, {"showAxes", js_g3d_showAxes, 1}, {"addSphere", js_g3d_addSphere, 6},
        {"addPlane", js_g3d_addPlane, 5},
    };
    for (auto &m : methods) { JS_SetPropertyStr(ctx, obj, m.name, JS_NewCFunction(ctx, m.fn, m.name, m.argc)); }
}

// ── JS 工厂 (仅 makeElement → makeElementHelper) ──
static JSValue js_g3d(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    if (JS_IsObject(props)) {
        JSValue idVal = JS_GetPropertyStr(ctx, props, "id");
        if (JS_IsUndefined(idVal)) {
            static std::atomic<uint32_t> g3d_id_counter{0};
            char buf[64];
            std::snprintf(buf, sizeof(buf), "__g3d_%u", g3d_id_counter++);
            JS_SetPropertyStr(ctx, props, "id", JS_NewString(ctx, buf));
        }
        JS_FreeValue(ctx, idVal);
    }
    JSValue obj = makeElementHelper(ctx, "G3D", props, JS_UNDEFINED);
    bindG3DMethods(ctx, obj);

    // 立即创建 C++ G3D, 指针存到 props (方法调用直达)
    auto *g3d = new G3D();
    JS_SetPropertyStr(ctx, props, "__g3d_ptr",
                      JS_NewFloat64(ctx, static_cast<double>(reinterpret_cast<uintptr_t>(g3d))));
    return obj;
}

// ── 创建器 (自 element_parser.cpp 搬迁) ──
static std::unique_ptr<View> createG3D(const JSValueRef &pv) {
    if (pv.hasProperty("__g3d_ptr")) {
        auto ptrVal = pv.getProperty("__g3d_ptr");
        JSContext *ctx = pv.context();
        double v;
        JS_ToFloat64(ctx, &v, ptrVal.raw());
        auto *existing = reinterpret_cast<G3D *>(static_cast<uintptr_t>(v));
        TypedPropMap meta;
        PropsExtractor ex(pv, &meta);
        existing->props = parseViewProps(ex);
        return std::unique_ptr<G3D>(existing);    // 树接管所有权
    }
    TypedPropMap meta;
    PropsExtractor ex(pv, &meta);
    auto v = std::make_unique<G3D>(parseViewProps(ex));
    v->propMeta = std::move(meta);
    applyBindings(v.get(), pv);
    return v;
}

// ── 自注册 ──
void registerG3DElement() {
    ElementSpec spec;
    spec.typeName = "G3D";
    spec.creator = createG3D;
    spec.jsFactoryName = "G3D";
    spec.jsFactoryFn = js_g3d;
    spec.jsFactoryArgc = 1;
    ElementParser::registerExtension(std::move(spec));
}