// ============================================================================
// 模块实现: kwik.bridge.element_parser
//
// 核心逻辑:
//   读取 JS 对象的 type / props / children 字段，递归构建 C++ View 组件树。
//   通过 类型注册表 (Registry Pattern) 按 type 字段分发创建对应组件，
//   替换原有的 if-else 链，使新增组件类型无需修改解析核心。
//
// 引用计数约定:
//   - parse(JSContext*, JSValueConst) 入口处 JS_DupValue，确保 JSValueRef
//     RAII 包装持有一个独立引用，避免析构时释放借用引用导致 refcount 下溢
//   - parseNode() 递归过程中 getProperty / getArrayElement 返回的 JSValueRef
//     已自行持有引用（来自 JS_GetProperty 的 refcount+1），析构时自动释放
// ============================================================================
module;
#include "quickjs.h"
module kwik.bridge.element_parser;
import kwik.bridge.props_parser;
import kwik.element.view;
import kwik.element.props;
import kwik.engine.js_value;
import kwik.element.text;
import kwik.element.button;
import kwik.layout.flex_layout;
import kwik.layout.grid_layout;
import kwik.layout.stack_layout;
import kwik.layout.list_layout;
import kwik.element.image;

import std;
// ============================================================================
// 类型注册表 — Meyer's Singleton
//
// 使用函数内静态变量模式 (C++11 §6.7/4):
//   - 首次调用 ElementParser::creators() 时构造
//   - 线程安全（C++11 保证局部静态变量初始化无竞态）
//   - 生命周期持续到程序结束
//   - 内置类型通过 InitBuiltinTypes 静态构造在 main() 之前完成注册
// ============================================================================
std::unordered_map<std::string, TypeCreator> &ElementParser::creators() {
    static std::unordered_map<std::string, TypeCreator> registry;
    return registry;
}
// ============================================================================
// 注册类型 —— 向注册表中插入或覆盖创建器
// ============================================================================
void ElementParser::registerType(const std::string &name, TypeCreator creator) {
    creators()[name] = std::move(creator);
}
// ============================================================================
// 模块静态初始化 —— 在 main() 之前自动注册内置组件类型
//
// 注册时机:
//   C++ 保证翻译单元内的静态对象先于任何函数调用构造。
//   此处 _init_builtin_types 的构造发生在 main() 之前，
//   因此 ElementParser::parse() 首次调用时注册表已就绪。
//
// 扩展方式:
//   后续新增 Text / Button / Image 等组件时，在各自实现文件的
//   静态初始化区调用 ElementParser::registerType() 即可，
//   无需修改本文件。
// ============================================================================
static struct InitBuiltinTypes {
    InitBuiltinTypes() {
        ElementParser::registerType("View",
                                    [](const JSValueRef &pv) { return std::make_unique<View>(parseViewProps(pv)); });
        ElementParser::registerType("Text", [](const JSValueRef &pv) {
            return std::make_unique<Text>(parseViewProps(pv), parseTextContent(pv));
        });
        ElementParser::registerType("Button", [](const JSValueRef &pv) {
            return std::make_unique<Button>(parseViewProps(pv), parseTextContent(pv), parseButtonState(pv));
        });
        // ── 布局容器 ───────────────────────────────────────
        ElementParser::registerType("Flex", [](const JSValueRef &pv) {
            return std::make_unique<FlexLayout>(parseViewProps(pv), parseContainerProps(pv));
        });
        ElementParser::registerType("Grid", [](const JSValueRef &pv) {
            return std::make_unique<GridLayout>(parseViewProps(pv), parseContainerProps(pv));
        });
        ElementParser::registerType(
            "Stack", [](const JSValueRef &pv) { return std::make_unique<StackLayout>(parseViewProps(pv)); });

        // ── 列表布局 ───────────────────────────────────────
        ElementParser::registerType("List", [](const JSValueRef &pv) {
            auto list = std::make_unique<ListLayout>(parseViewProps(pv), parseContainerProps(pv));
            if (pv.hasProperty("header")) {
                auto hdr = pv.getProperty("header");
                if (hdr.isObject()) {
                    JSContext *c = hdr.context();
                    JSValue dup = JS_DupValue(c, hdr.raw());
                    JSValueRef node(c, dup);
                    list->header = ElementParser::parseNode(node);
                }
            }
            if (pv.hasProperty("footer")) {
                auto ftr = pv.getProperty("footer");
                if (ftr.isObject()) {
                    JSContext *c = ftr.context();
                    JSValue dup = JS_DupValue(c, ftr.raw());
                    JSValueRef node(c, dup);
                    list->footer = ElementParser::parseNode(node);
                }
            }
            return list;
        });

        // ── 图像组件 ───────────────────────────────────────
        ElementParser::registerType("Image", [](const JSValueRef &pv) {
            return std::make_unique<Image>(parseViewProps(pv), parseImageProps(pv));
        });
    }
} _init_builtin_types;
// ============================================================================
// 公开接口 - parse(JSContext* ctx, JSValueConst value)
//
// 这是外部代码（如 view_example.cpp）调用的主入口。
//
// 引用计数说明:
//   jsContext.getRootView() 返回 rootView 的副本 (JSValue 按值返回)，
//   这是一个借用引用（无 refcount 变更）。若直接传入 JSValueRef 构造，
//   JSValueRef 析构时会 JS_FreeValue → refcount 下溢 → 断言失败。
//   因此此处显式 JS_DupValue 获取一个独立引用，转移给 JSValueRef RAII。
// ============================================================================
std::unique_ptr<View> ElementParser::parse(JSContext *ctx, JSValueConst value) {
    JSValueRef jsVal(ctx, JS_DupValue(ctx, value));
    return parse(jsVal);
}
// ============================================================================
// 公开接口 - parse(const JSValueRef& jsVal)
//
// 入参 jsVal 应是持有合法引用计数的 JSValueRef（来自上述重载的构造）。
// 此处先校验类型再进入递归解析。
// ============================================================================
std::unique_ptr<View> ElementParser::parse(const JSValueRef &jsVal) {
    if (!jsVal.isObject() || jsVal.isNull()) { return nullptr; }
    return parseNode(jsVal);
}
// ============================================================================
// 私有 - parseNode(const JSValueRef& jsVal)
//
// 深度优先递归，处理单个 JS 组件节点的完整解析流程:
//
//   JS 节点结构 { type, props, children }
//       │
//       ├─ 1. type   → string, 查找注册表 → creator(props) → C++ View 子类
//       ├─ 2. props  → object, parseViewProps() → ViewProps 结构体
//       └─ 3. children → array, 对每个元素递归 parseNode()
//
// 边界情况:
//   - type 为空或缺失 → 返回 nullptr（无效节点）
//   - type 未注册     → 降级为 View 创建（保证不丢失节点）
//   - children 非数组  → 跳过子节点解析（视为叶子节点）
// ============================================================================
std::unique_ptr<View> ElementParser::parseNode(const JSValueRef &jsVal) {
    // ── 1. 读取组件类型 ──────────────────────────────────────────────
    auto typeVal = jsVal.getProperty("type");
    std::string type = typeVal.toString();
    if (type.empty()) { return nullptr; }
    // ── 2. 获取 props JS 对象 ─────────────────────────────────────
    auto propsVal = jsVal.getProperty("props");
    // ── 3. 查注册表分发创建 C++ 组件 ──────────────────────────
    std::unique_ptr<View> element;
    auto registry = creators();
    auto it = registry.find(type);
    if (it != registry.end()) {
        element = it->second(propsVal);
    } else {
        element = std::make_unique<View>(parseViewProps(propsVal));
    }
    // ── 4. 递归解析子节点 ────────────────────────────────────────────
    auto childrenVal = jsVal.getProperty("children");
    if (childrenVal.isArray()) {
        int len = childrenVal.getArrayLength();
        for (int i = 0; i < len; ++i) {
            auto child = parseNode(childrenVal.getArrayElement(i));
            if (child) { element->addChild(std::move(child)); }
        }
    }
    // ── 5. 提取事件处理器 ────────────────────────────────────────
    if (element && propsVal.isObject()) {
        JSContext *ctx_ = propsVal.context();
        auto tryBind = [&](const char *propName) {
            if (propsVal.hasProperty(propName)) {
                auto handler = propsVal.getProperty(propName);
                if (JS_IsFunction(ctx_, handler.raw())) { element->handlers.bind(ctx_, propName, handler.raw()); }
            }
        };
        tryBind("onClick");
        tryBind("onLongPress");
        tryBind("onHoverEnter");
        tryBind("onHoverLeave");
    }
    return element;
}

// ============================================================================
// 公开接口 - printTree 打印完整 View 组件树
// ============================================================================
void ElementParser::printTree(const View *view, int depth, const std::string &prefix) {
    if (!view) return;
    const auto &p = view->props;
    std::print("{}{} [{:.0f}x{:.0f}]", prefix, view->typeName(), view->frame.width, view->frame.height);
    if (p.background.isVisible())
        std::print(" bg=#{:02X}{:02X}{:02X} a={}", p.background.r, p.background.g, p.background.b, p.background.a);
    if (p.borderRadius > 0) std::print(" radius={}", p.borderRadius);
    if (p.borderWidth > 0)
        std::print(" border={} #{:02X}{:02X}{:02X}", p.borderWidth, p.borderColor.r, p.borderColor.g, p.borderColor.b);
    if (p.padding.top > 0 || p.padding.left > 0 || p.padding.bottom > 0 || p.padding.right > 0)
        std::print(" padding=[{:.0f},{:.0f},{:.0f},{:.0f}]", p.padding.top, p.padding.right, p.padding.bottom,
                   p.padding.left);
    if (p.margin.top > 0 || p.margin.left > 0 || p.margin.bottom > 0 || p.margin.right > 0)
        std::print(" margin=[{:.0f},{:.0f},{:.0f},{:.0f}]", p.margin.top, p.margin.right, p.margin.bottom,
                   p.margin.left);
    if (const auto *t = dynamic_cast<const Text *>(view)) {
        const auto &tc = t->textContent();
        if (!tc.text.empty()) {
            std::print(" text=\"{}\"", tc.text.c_str());
            std::print(" fontSize={:.0f}", tc.fontSize);
            if (tc.textColor.isVisible())
                std::print(" color=#{:02X}{:02X}{:02X}", tc.textColor.r, tc.textColor.g, tc.textColor.b);
        }
    }
    if (p.opacity < 1.0f) std::print(" opacity={:.2f}", p.opacity);
    std::println();
    for (size_t i = 0; i < view->children.size(); ++i) {
        bool last = (i == view->children.size() - 1);
        std::string childPrefix = prefix;
        childPrefix += last ? "    " : "│   ";
        childPrefix += last ? "└── " : "├── ";
        printTree(view->children[i].get(), depth + 1, childPrefix);
    }
}