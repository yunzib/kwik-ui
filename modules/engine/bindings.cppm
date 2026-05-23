// kwik.engine.bindings.cppm
module;

#include "quickjs.h"
#include <functional>

export module kwik.engine.bindings;

// ---------- 用于触发渲染的回调 ----------
export using RenderCallback = std::function<void()>;

// ---------- 初始化 / 注册函数 ----------
/**
 * @brief 注册 State 类到指定的 JSContext，内部保存类 ID
 */
export JSValue register_state_class(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/**
 * @brief 注册 Channel 类到指定的 JSContext，内部保存类 ID
 */
export JSValue register_channel_class(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/**
 * @brief 设置全局渲染回调（在 State 属性变更时调用）
 * @param callback 可调用对象（如 lambda）
 */
export void set_render_callback(RenderCallback callback);

/**
 * @brief 注册 kwikui 原生模块到指定的 JSContext
 *        - 注册 State/Channel 类（如果尚未注册）
 *        - 创建并注册 "kwikui" C 模块，导出 View/Text/State/Channel
 *        - 为 State 和 Channel 添加原型方法（update/send/receive）
 * @param ctx 目标 JSContext
 * @return true 成功，false 失败
 */
export JSModuleDef *register_kwikui_module(JSContext *ctx);

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
export JSValue js_channel_constructor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
export JSValue js_channel_send(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
export JSValue js_channel_receive(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);