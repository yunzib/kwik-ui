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

