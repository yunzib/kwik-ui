module;
#include "quickjs.h"
export module kwik.bridge.prop_bus;
/**
 * @brief 向 kwikui JS 模块注册 getProp / setProp
 *
 * 在 QuickJSContext 构造之后、JS 执行之前调用。
 * @param ctx QuickJS 上下文 (需 kwikuiModule 已注册)
 */
export void register_prop_bus(JSContext *ctx);
