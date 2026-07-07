// kwik.engine.bindings.cppm
module;

#include "quickjs.h"

export module kwik.bridge.bindings;
import kwik.engine.vm_callbacks;
import kwik.engine.context;

/**
 * @brief 注册 kwikui 原生模块到指定的 QuickJSContext
 *        - 创建并注册 "kwikui" C 模块，导出 View/Text/State/Channel
 *        - 内部调用 setKwikuiModule 保存模块指针
 * @param qctx 目标 QuickJSContext
 * @return true 成功，false 失败
 */
export bool register_kwikui_module(QuickJSContext &qctx);

// ---------- 导出给 JS 调用的 C 函数 ----------
export JSValue js_view(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
export JSValue js_text(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
export JSValue js_button(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
export JSValue js_flex(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
export JSValue js_grid(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
export JSValue js_stack(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
export JSValue js_list(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
export JSValue js_image(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

export JSValue js_state_constructor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
export JSValue js_state_update(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
// export JSValue js_channel_constructor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
// export JSValue js_channel_send(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
// export JSValue js_channel_receive(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);