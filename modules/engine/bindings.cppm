// kwik.engine.bindings.cppm
module;

#include "quickjs.h"
#include <functional>

export module kwik.engine.bindings;

// ---------- 用于触发渲染的回调 ----------
export using RenderCallback = std::function<void()>;

/**
 * @brief 增量更新回调
 *
 * state_set_property 在写入 JS 对象后优先调用此回调。
 * 若回调返回 true，表示增量更新已完成，跳过全量重建。
 *
 * @param statePtr StateData* 的 void* 指针（用于 BindingRegistry 查表）
 * @param key      变更的属性名（JS_AtomToCString 结果）
 * @param ctx      QuickJS 上下文
 * @param newValue 新的 JS 值（调用方保证生命周期仅限函数内）
 * @return true  增量更新已处理，无需全量重建
 * @return false 未处理，回退到 render_callback
 */
export using IncrementalCallback = bool (*)(void* statePtr, const char* key,
                                            JSContext* ctx, JSValueConst newValue);

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
 * @brief 设置增量更新回调
 * @param callback 函数指针，可为 nullptr
 *
 * state_set_property 中先尝试 incremental_callback，
 * 只有未设置或返回 false 时才走 render_callback → rebuildTree。
 */
export void set_incremental_callback(IncrementalCallback callback);

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