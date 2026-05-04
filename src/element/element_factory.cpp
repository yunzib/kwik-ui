// ============================================================================
// 模块实现: kwik.element.element_factory
// 核心逻辑: 读取 JS 对象的 type/props/children 字段, 递归构建 C++ View 树
// ============================================================================
module;
#include "quickjs.h"
module kwik.element.element_factory;
import kwik.element.props_parser;
import kwik.element.view;
import kwik.element.props;
import kwik.engine.js_value;
import std;

// ============================================================================
// 前向声明
// ============================================================================
static std::unique_ptr<View> parseNode(const JSValueRef& jsVal);
// ============================================================================
// 公开接口
// ============================================================================
std::unique_ptr<View> ElementFactory::parse(JSContext* ctx, JSValueConst value) {
    JSValueRef jsVal(ctx, value);
    return parse(jsVal);
}
std::unique_ptr<View> ElementFactory::parse(const JSValueRef& jsVal) {
    if (!jsVal.isObject() || jsVal.isNull()) {
        return nullptr;
    }
    return parseNode(jsVal);
}
// ============================================================================
// 私有: 递归解析单节点
// ============================================================================
static std::unique_ptr<View> parseNode(const JSValueRef& jsVal) {
    // ── 1. 读取组件类型 ──────────────────────────────────────────────
    auto typeVal = jsVal.getProperty("type");
    std::string type = typeVal.toString();
    // ── 2. 解析 props 属性 ───────────────────────────────────────────
    //     复用 props_parser 模块的 parseViewProps
    ViewProps props = parseViewProps(jsVal.getProperty("props"));
    // ── 3. 按 type 分发创建对应 C++ 组件 ─────────────────────────────
    //     后续新增组件类型只需在此添加分支
    std::unique_ptr<View> element;
    if (type == "Text") {
        // TODO: 替换为 Text 类实例 (继承 View, 覆写 onDraw 绘制文字)
        element = std::make_unique<View>(std::move(props));
    } else if (type == "Button") {
        // TODO: 替换为 Button 类实例
        element = std::make_unique<View>(std::move(props));
    } else if (type == "Image") {
        // TODO: 替换为 Image 类实例
        element = std::make_unique<View>(std::move(props));
    } else {
        // 默认创建 View (覆盖 "View" 及未识别类型)
        element = std::make_unique<View>(std::move(props));
    }
    // ── 4. 递归解析子节点 ────────────────────────────────────────────
    auto childrenVal = jsVal.getProperty("children");
    if (childrenVal.isArray()) {
        int len = childrenVal.getArrayLength();
        for (int i = 0; i < len; i++) {
            auto child = parseNode(childrenVal.getArrayElement(i));
            if (child) {
                element->addChild(std::move(child));
            }
        }
    }
    return element;
}
