/** @brief G2D 命令式 2D 绘制组件的 JS 绑定与插件式注册。 */
export module kwik.bridge.g2d;

/**
 * @brief 注册 G2D 元素 (creator + jsFactory)
 *
 * 由 register_kwikui_module 显式调用 (G2D 为内置组件, 显式调用制造强引用,
 * 避免静态库链接器丢弃本对象文件), 须在 JS_NewCModule 之前调用。
 */
export void registerG2DElement();