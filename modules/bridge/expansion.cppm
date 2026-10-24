/** @brief Expansion 可展开容器原子：JS 工厂 / 事件接线 / 自注册（新注册方案常规组件首例）。 */
export module kwik.bridge.expansion;

/**
 * @brief 注册 Expansion 元素（ElementSpec 四钩子自包含）
 *
 * 由 register_kwikui_module 显式调用（内部必备件，显式调用制造强引用,
 * 避免静态库链接器丢弃本对象文件，G2D 同款），须在 JS_NewCModule 之前调用。
 */
export void registerExpansionElement();
