// ============================================================================
// 模块接口: kwik.element.element_factory
// 用途: 将 JS 侧 makeElement() 产生的 {type, props, children} 对象树
//       转化为 C++ View 组件树，按 type 分发创建对应组件
//       作为 JS 模块输出到 渲染线程 之间的解析转换层
// ============================================================================
module;
#include "quickjs.h"
export module kwik.element.element_factory;
import kwik.element.view;
import kwik.element.props;
import kwik.element.props_parser;
import kwik.engine.js_value;
import std;

/**
 * @brief Element 工厂类 —— JS 对象树 → C++ View 组件树
 *
 * 职责：
 * 1. 接收 QuickJSContext 执行产生的 JS 根对象 (makeElement 格式)
 * 2. 递归解析 {type, props, children} 结构
 * 3. 根据 type 字段分发创建对应 C++ 组件 (View/Text/Button...)
 * 4. 返回可渲染的 View 组件树根节点
 *
 * 调用链：
 *   QuickJSContext → eval(jsCode) → 保存 rootView
 *   ElementFactory::parse(ctx, rootView) → std::unique_ptr<View>
 *   View::draw() → Graphics → CommandBuffer → RenderThread
 */
class ElementFactory {
public:
    /**
     * @brief 从 JS 值解析并创建 Element 树
     * @param ctx   QuickJS 上下文
     * @param value JS 对象值 (makeElement 产物 或 根对象)
     * @return 组件树根节点, 失败返回 nullptr
     */
    static std::unique_ptr<View> parse(JSContext* ctx, JSValueConst value);
    /**
     * @brief 从 JS 值解析并创建 Element 树 (JSValueRef 包装版本)
     */
    static std::unique_ptr<View> parse(const JSValueRef& jsVal);
};
