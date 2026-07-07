module;
#include <functional>

#include "quickjs.h"

export module kwik.engine.vm_callbacks;

/**
 * @brief 渲染回调类型 — 当 State 属性变更时触发 UI 重建
 */
export using RenderCallback = std::function<void()>;

/**
 * @brief 增量更新回调 — 拦截 State 属性变更，查询 BindingRegistry
 *        若已处理则跳过全量重建
 * @param statePtr  StateData* 的 void* 指针（用作 BindingRegistry 查找 key）
 * @param key       变更的属性名
 * @param ctx       QuickJS 上下文指针
 * @param newValue  新的 JS 值（生命周期仅限函数调用内）
 * @return true=已处理，false=未处理回退到 RenderCallback
 */
export using IncrementalCallback = bool (*)(void* statePtr, const char* key,
                                            JSContext* ctx, JSValueConst newValue);

/**
 * @brief 设置渲染回调（由 QuickJSContext 构造时注册）
 */
export void set_render_callback(RenderCallback callback);

/**
 * @brief 设置增量更新回调（由 binding_registry 在 setRegisteredRegistry 时注册）
 */
export void set_incremental_callback(IncrementalCallback callback);

/**
 * @brief 获取渲染回调（供 bridge bindings 中 state_set_property 使用）
 */
export RenderCallback get_render_callback();

/**
 * @brief 获取增量更新回调（供 bridge bindings 中 state_set_property 使用）
 */
export IncrementalCallback get_incremental_callback();