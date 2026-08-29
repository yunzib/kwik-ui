/** @brief Video 扩展: JS 工厂 / 方法 / 事件接线 / 自注册。 */
module;
#include "quickjs.h"
#include <cstdio>
#include <atomic>

module kwik.ext.video;

import kwik.bridge.element_parser; // ElementParser::registerExtension / applyBindings
import kwik.bridge.element_spec;   // ElementSpec
import kwik.bridge.props_parser;   // PropsExtractor / parseViewProps
import kwik.bridge.bindings;       // makeElementHelper
import kwik.bridge.event_adapter;  // attachJsHandlers
import kwik.engine.context;        // QuickJSContext (getUserPointer)
import kwik.element.view;          // View::findById
import kwik.element.typed_prop;    // TypedPropMap
import kwik.engine.js_value;

import std;

// ═══════════════════════════════════════════════════════════
// 属性解析 (对齐 parseLineProps 模式, 不污染内置 props_parser)
// ═══════════════════════════════════════════════════════════
static VideoProps parseVideoProps(PropsExtractor &ex) {
    VideoProps r;
    ex.get("src", r.src);
    ex.get("autoplay", r.autoplay);
    ex.get("loop", r.loop);
    ex.get("muted", r.muted);
    return r;
}

// ═══════════════════════════════════════════════════════════
// 创建器 (creator) — 复用 parseViewProps + parseVideoProps
// ═══════════════════════════════════════════════════════════
static std::unique_ptr<View> createVideo(const JSValueRef &pv) {
    TypedPropMap meta;
    PropsExtractor ex(pv, &meta);
    auto v = std::make_unique<Video>(parseViewProps(ex), parseVideoProps(ex));
    v->propMeta = std::move(meta);
    applyBindings(v.get(), pv);    // State 双向绑定 (公开化后的 applyBindings)
    return v;
}

// ═══════════════════════════════════════════════════════════
// reconcile 专有属性重解析 (reconcileProps 钩子)
// ═══════════════════════════════════════════════════════════
static void reconcileVideo(View *view, PropsExtractor &ex) {
    static_cast<Video *>(view)->applyVideoProps(parseVideoProps(ex));
}

// ═══════════════════════════════════════════════════════════
// JS 方法: 从 this.props.id 在树中查 Video* (等价 G3D_FROM_THIS 的
// findById 回退分支, 避免 eager 创建导致 reconcile 泄漏)
// ═══════════════════════════════════════════════════════════
static Video *videoFromThis(JSContext *ctx, JSValueConst this_val) {
    JSValue p = JS_GetPropertyStr(ctx, this_val, "props");
    JSValue id = JS_GetPropertyStr(ctx, p, "id");
    const char *idStr = JS_ToCString(ctx, id);
    Video *v = nullptr;
    auto *qctx = static_cast<QuickJSContext *>(JS_GetContextOpaque(ctx));
    View *root = qctx ? static_cast<View *>(qctx->getUserPointer()) : nullptr;
    if (root && idStr) v = dynamic_cast<Video *>(root->findById(idStr));
    if (idStr) JS_FreeCString(ctx, idStr);
    JS_FreeValue(ctx, id);
    JS_FreeValue(ctx, p);
    return v;
}

static JSValue js_video_play(JSContext *ctx, JSValueConst this_val, int, JSValueConst *) {
    if (auto *v = videoFromThis(ctx, this_val)) v->play();
    return JS_UNDEFINED;
}
static JSValue js_video_pause(JSContext *ctx, JSValueConst this_val, int, JSValueConst *) {
    if (auto *v = videoFromThis(ctx, this_val)) v->pause();
    return JS_UNDEFINED;
}

static JSValue js_video_seek(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (auto *v = videoFromThis(ctx, this_val)) {
        if (argc > 0) {
            double t = 0;
            JS_ToFloat64(ctx, &t, argv[0]);    // 3 参签名: ctx, 输出指针, 输入值
            v->seek((float)t);
        }
    }
    return JS_UNDEFINED;
}

static void bindVideoMethods(JSContext *ctx, JSValue obj) {
    JS_SetPropertyStr(ctx, obj, "play", JS_NewCFunction(ctx, js_video_play, "play", 0));
    JS_SetPropertyStr(ctx, obj, "pause", JS_NewCFunction(ctx, js_video_pause, "pause", 0));
    JS_SetPropertyStr(ctx, obj, "seek", JS_NewCFunction(ctx, js_video_seek, "seek", 1));
}

// ═══════════════════════════════════════════════════════════
// JS 工厂: Video({...}) → { type, props, children } + 方法绑定
// ═══════════════════════════════════════════════════════════
static JSValue js_video(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    if (JS_IsObject(props)) {
        // 保证有 id, 方法调用 (play/pause/seek) 才能按 id 定位 C++ Video
        JSValue idVal = JS_GetPropertyStr(ctx, props, "id");
        if (JS_IsUndefined(idVal)) {
            static std::atomic<uint32_t> counter{0};
            char buf[64];
            std::snprintf(buf, sizeof(buf), "__video_%u", counter++);
            JS_SetPropertyStr(ctx, props, "id", JS_NewString(ctx, buf));
        }
        JS_FreeValue(ctx, idVal);
    }
    JSValue obj = makeElementHelper(ctx, "Video", props, JS_UNDEFINED);
    bindVideoMethods(ctx, obj);
    return obj;
}

// ═══════════════════════════════════════════════════════════
// 事件接线 (v1 复用通用指针/onChange; 视频专属事件
// onEnd/onTimeUpdate/onError 待 Video 增加 std::function 槽位后扩展)
// ═══════════════════════════════════════════════════════════
static void attachVideoHandlers(View &view, const JSValueRef &props) {
    attachJsHandlers(view, props);
}

// ═══════════════════════════════════════════════════════════
// 自注册 — 组装 ElementSpec 交给 ElementParser::registerExtension
// ═══════════════════════════════════════════════════════════
void registerVideoElement() {
    ElementSpec spec;
    spec.typeName = "Video";
    spec.creator = createVideo;
    spec.reconcileProps = reconcileVideo;
    spec.attachHandlers = attachVideoHandlers;
    spec.jsFactoryName = "Video";
    spec.jsFactoryFn = js_video;
    spec.jsFactoryArgc = 1;
    ElementParser::registerExtension(std::move(spec));
}