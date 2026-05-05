// ============================================================================
// 模块接口: kwik.bridge.element_parser
// 用途: 将 JS 侧 makeElement() 产生的 {type, props, children} 对象树
//       转化为 C++ View 组件树，通过类型注册表按 type 分发创建对应组件
//       处于 JS 执行引擎 与 渲染线程 之间的解析转换层
// ============================================================================
module;
#include "quickjs.h"
export module kwik.bridge.element_parser;
import kwik.element.view;
import kwik.element.props;
import kwik.bridge.props_parser;
import kwik.engine.js_value;
import std;
// ============================================================================
// 类型别名
// ============================================================================
/**
 * @brief 组件类型创建器签名
 *
 * 接收已解析完成的 ViewProps（右值），返回对应 C++ 组件实例的 unique_ptr。
 * 例如: [](ViewProps&& p) { return std::make_unique<View>(std::move(p)); }
 *
 * 使用 std::function 而非函数指针，以支持 lambda 捕获（后续可扩展为含状态的工厂）
 */
export using TypeCreator = std::function<std::unique_ptr<View>(ViewProps&&)>;
// ============================================================================
// ElementParser 类声明
// ============================================================================
/**
 * @brief JS 对象树 → C++ View 组件树 解析器
 *
 * 职责:
 *  1. 接收 QuickJS 模块执行产物 —— makeElement() 生成的 {type, props, children} 对象树
 *  2. 深度优先递归遍历，逐节点解析
 *  3. 通过类型注册表 (Registry Pattern) 按 type 分发创建 C++ 组件
 *  4. 返回完整的可渲染 View 组件树根节点
 *
 * 类型注册机制:
 *  内置组件 (View 等) 在静态初始化阶段自动注册。
 *  后续新增组件类型 (Text / Button / Image) 无需修改本模块，
 *  只需在其初始化代码中调用 registerType() 即可加入注册表。
 *
 * 调用链:
 *   QuickJSContext::evalFile("app.js")               // 1. 执行 JS 模块
 *   jsContext.getRootView()                           // 2. 获取 default 导出 (JSValue)
 *   ElementParser::parse(ctx, rootView)              // 3. 解析 → C++ View 树
 *   viewTree->layout(screenRect)                      // 4. 布局
 *   viewTree->draw(graphics)                          // 5. 绘制 → CommandBuffer
 *   RenderThread → RenderBackend                      // 6. 渲染
 */
export class ElementParser {
public:
    // ==================== 类型注册接口 ====================
    /**
     * @brief 注册组件类型创建器
     *
     * @param name    组件类型名称，对应 JS 侧 type 字段 ("View", "Text" 等)
     * @param creator 创建器函数，接收 ViewProps&&，返回组件实例的 unique_ptr
     *
     * 若同名类型已注册，新创建器将覆盖旧值。
     */
    static void registerType(const std::string& name, TypeCreator creator);

    // ==================== 调试接口 ====================
    /**
     * @brief 打印 View 组件树（递归，含所有属性）
     */
    static void printTree(const View* view, int depth = 0, const std::string& prefix = " ");
    
    // ==================== 解析接口 ====================
    /**
     * @brief 从 JS 值解析组件树（JSContext + JSValueConst 版本）
     *
     * @param ctx   QuickJS 上下文指针
     * @param value JS 对象值（通常为 QuickJSContext::getRootView() 的返回值）
     * @return 组件树根节点，若传入非 object / null 则返回 nullptr
     *
     * 内部会 JS_DupValue 以持有独立引用，安全包裹为 JSValueRef
     */
    static std::unique_ptr<View> parse(JSContext* ctx, JSValueConst value);
    /**
     * @brief 从 JSValueRef 解析组件树（JSValueRef 版本）
     *
     * @param jsVal 已持有合法引用计数的 JSValueRef 包装对象
     * @return 组件树根节点
     */
    static std::unique_ptr<View> parse(const JSValueRef& jsVal);
private:
    // ==================== 内部实现 ====================
    /**
     * @brief 获取类型注册表（Meyer's Singleton）
     *
     * C++11 保证局部静态变量的初始化是线程安全的，
     * 且首次调用时构造，确保内置类型在解析前就绪
     */
    static std::unordered_map<std::string, TypeCreator>& creators();
    /**
     * @brief 深度优先递归解析单个 JS 节点
     *
     * 解析步骤:
     *  1. 读取 type 字段 (组件类型名称)
     *  2. 解析 props 字段 (调用 props_parser)
     *  3. 查注册表创建 C++ 组件
     *  4. 递归解析 children 数组
     *
     * @param jsVal 当前节点的 JSValueRef 引用
     * @return 解析完成的组件节点（含所有子节点）
     */
    static std::unique_ptr<View> parseNode(const JSValueRef& jsVal);
};