// ============================================================================
// state_binding.cppm — JS 状态绑定工厂
//
// StateBinding 抽象接口已下移至 kwik.core.binding (element 层直接依赖 core),
// 本模块仅保留 QuickJS 实现的工厂入口, 并通过 export import 转发接口,
// 使既有调用方 (bridge/element_parser) 无需修改。
// ============================================================================

module;
#include "quickjs.h"

export module kwik.engine.state_binding;

export import kwik.core.binding;    // 转发 StateBinding 抽象接口

import std;

/**
 * @brief 创建 JS 状态绑定实例
 * @param ctx      QuickJS 上下文
 * @param stateObj State exotic 对象（函数内部会 dup）
 * @return unique_ptr<StateBinding>
 *
 * 调用方通过 StateBinding 抽象接口使用，不依赖 JSStateBinding 具体类型。
 */
export std::unique_ptr<StateBinding> createJSBinding(JSContext *ctx, JSValue stateObj);