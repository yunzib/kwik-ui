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

/**
 * @brief 通用组件创建入口 (公开化供扩展插件复用)
 *
 * 与内置 js_xxx 工厂内部使用的 makeElement 同源:
 * 返回 { type, props, children } 对象, 并统一处理 ref(state,key) 绑定解析。
 * 扩展组件 (Video) 的 JS 工厂调用此函数造元素描述符。
 */
export JSValue makeElementHelper(JSContext *ctx, const char *type, JSValueConst props, JSValueConst children);