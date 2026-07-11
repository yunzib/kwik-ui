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
import kwik.core.log;
module kwik.bridge.element_parser;
import kwik.bridge.props_parser;
import kwik.element.view;
import kwik.element.rootview;
import kwik.core.props;
import kwik.engine.js_value;
import kwik.layout.flex_layout;
import kwik.layout.grid_layout;
import kwik.layout.stack_layout;
import kwik.layout.list_layout;
import kwik.element.image;
import kwik.engine.state_binding;
import kwik.element.typed_prop;
import kwik.bridge.binding_registry;
// import kwik.element.slider;
// import kwik.element.progressbar;
import kwik.element.switch_button;
// import kwik.element.line;
// import kwik.element.spinner;
// import kwik.element.table;
// import kwik.element.textview;
import kwik.element.text;
import kwik.element.button;
import kwik.element.input;
import kwik.element.radiobutton;
import kwik.layout.radio_group;
import kwik.element.checkbox;
// import kwik.element.textarea;
// import kwik.element.dropdown;

import std;

/**
 * @brief 统一绑定注入
 *
 * 遍历 View::propMeta 中 hasBinding=true 的属性，逐一从 JS props
 * 读取 __bind_{propName}State / __bind_{propName}Key，
 * 调用 View::setBinding() 建立双向绑定。
 *
 * 替代原先 Input/Checkbox/RadioGroup/TextArea/Dropdown 各自手写的
 * if (pv.hasProperty("__bind_*Key")) 检测分支，消除重复代码。
 *
 * Dropdown 特殊处理：resolveRefProp 已将 value 替换为 State 当前值，
 * 但 Dropdown 初始化时需通过 setProperty("value", ...) 将字符串值
 * 映射为 selectedIndex，否则绑定值正确但索引未同步。
 *
 *（模板版本）
 *
 * T 必须是 Input / Checkbox / RadioGroup / TextArea / Dropdown 之一，
 * 它们都有 setBinding() 方法。View 基类没有此方法。
 *
 * propMeta 是 View 的公共成员，通过 view->propMeta 访问。
 */
template <typename T>
static void applyBindings(T *view, const JSValueRef &pv) {
    auto &meta = view->propMeta;
    JSContext *ctx = pv.context();
    meta.forEachBinding([&](const std::string &propName, const PropEntry &) {
        std::string stateName = "__bind_" + propName + "State";
        std::string keyName = "__bind_" + propName + "Key";
        auto stateVal = pv.getProperty(stateName.c_str());
        auto keyVal = pv.getProperty(keyName.c_str());
        if (!stateVal.isUndefined() && !keyVal.isUndefined() && !JS_IsNull(stateVal.raw())) {
            view->setBinding(createJSBinding(ctx, stateVal.raw()), keyVal.toString());

            // 注册到 BindingRegistry，使 state 变更可增量更新到此 View
            if (auto *reg = getRegisteredRegistry()) {
                void *statePtr = JS_VALUE_GET_PTR(stateVal.raw());
                reg->bind(statePtr, keyVal.toString(), view, propName);
            }

            if (view->type() == ElementType::Dropdown && pv.hasProperty("value")) {
                std::string val = pv.getProperty("value").toString();
                if (!val.empty()) view->setProperty("value", val.c_str());
            }
        }
    });
}

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
// ═══════════════════════════════════════════════════════════════════════
// TypeCreator 列表 — 全部改为 PropsExtractor + applyBindings
// ═══════════════════════════════════════════════════════════════════════

static struct InitBuiltinTypes {
    InitBuiltinTypes() {
        ElementParser::registerType("View", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            return std::make_unique<View>(parseViewProps(ex));
        });

        ElementParser::registerType("Root", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            return std::make_unique<RootView>(parseViewProps(ex));
        });

        ElementParser::registerType("Text", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            return std::make_unique<Text>(parseViewProps(ex), parseTextContent(ex));
        });

        ElementParser::registerType("Button", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            return std::make_unique<Button>(parseViewProps(ex), parseTextContent(ex), parseButtonState(ex));
        });

        ElementParser::registerType("Flex", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            return std::make_unique<FlexLayout>(parseViewProps(ex), parseContainerProps(ex));
        });

        ElementParser::registerType("Grid", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            return std::make_unique<GridLayout>(parseViewProps(ex), parseContainerProps(ex));
        });

        ElementParser::registerType("Stack", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            return std::make_unique<StackLayout>(parseViewProps(ex));
        });

        ElementParser::registerType("List", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto list = std::make_unique<ListLayout>(parseViewProps(ex), parseContainerProps(ex));
            if (pv.hasProperty("header") && pv.getProperty("header").isObject()) {
                auto hdr = pv.getProperty("header");
                JSContext *c = hdr.context();
                JSValue dup = JS_DupValue(c, hdr.raw());
                JSValueRef node(c, dup);
                list->header = ElementParser::parseNode(node);
            }
            if (pv.hasProperty("footer") && pv.getProperty("footer").isObject()) {
                auto ftr = pv.getProperty("footer");
                JSContext *c = ftr.context();
                JSValue dup = JS_DupValue(c, ftr.raw());
                JSValueRef node(c, dup);
                list->footer = ElementParser::parseNode(node);
            }
            return list;
        });

        ElementParser::registerType("Image", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            return std::make_unique<Image>(parseViewProps(ex), parseImageProps(ex));
        });

        // ── Input — 绑定注入统一由 applyBindings 处理 ──
        ElementParser::registerType("Input", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto input = std::make_unique<Input>(parseViewProps(ex), parseInputProps(ex));
            input->propMeta = std::move(meta);
            applyBindings(input.get(), pv);
            return input;
        });

        ElementParser::registerType("RadioButton", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            return std::make_unique<RadioButton>(parseViewProps(ex), parseTextContent(ex),
            parseRadioButtonProps(ex));
        });

        // ── RadioGroup ──
        ElementParser::registerType("RadioGroup", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto rg = std::make_unique<RadioGroup>(parseViewProps(ex), parseRadioGroupProps(ex));
            rg->propMeta = std::move(meta);
            applyBindings(rg.get(), pv);
            return rg;
        });

        // ── Checkbox ──
        ElementParser::registerType("Checkbox", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto checkbox =
                std::make_unique<Checkbox>(parseViewProps(ex), parseTextContent(ex), parseCheckboxProps(ex));
            checkbox->propMeta = std::move(meta);
            applyBindings(checkbox.get(), pv);
            Log::debug("Checkbox created: id={}", checkbox->getProperty("id"));
            return checkbox;
        });

        // // ── TextArea ──
        // ElementParser::registerType("TextArea", [](const JSValueRef &pv) {
        //     TypedPropMap meta;
        //     PropsExtractor ex(pv, &meta);
        //     auto ta = std::make_unique<TextArea>(parseViewProps(ex), parseTextAreaProps(ex));
        //     ta->propMeta = std::move(meta);
        //     applyBindings(ta.get(), pv);
        //     return ta;
        // });

        // // ── Dropdown ──
        // ElementParser::registerType("Dropdown", [](const JSValueRef &pv) {
        //     TypedPropMap meta;
        //     PropsExtractor ex(pv, &meta);
        //     auto dd = std::make_unique<Dropdown>(parseViewProps(ex), parseDropdownProps(ex));
        //     dd->propMeta = std::move(meta);
        //     applyBindings(dd.get(), pv);
        //     return dd;
        // });

        // // ── Slider ──
        // ElementParser::registerType("Slider", [](const JSValueRef &pv) {
        //     TypedPropMap meta;
        //     PropsExtractor ex(pv, &meta);
        //     auto slider = std::make_unique<Slider>(parseViewProps(ex), parseSliderProps(ex));
        //     slider->propMeta = std::move(meta);
        //     applyBindings(slider.get(), pv);
        //     return slider;
        // });

        // // ── ProgressBar ──
        // ElementParser::registerType("ProgressBar", [](const JSValueRef &pv) {
        //     TypedPropMap meta;
        //     PropsExtractor ex(pv, &meta);
        //     auto pb = std::make_unique<ProgressBar>(parseViewProps(ex), parseProgressBarProps(ex));
        //     pb->propMeta = std::move(meta);
        //     applyBindings(pb.get(), pv);
        //     return pb;
        // });

        // ── Switch ──
        ElementParser::registerType("Switch", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto sw = std::make_unique<Switch>(parseViewProps(ex), parseSwitchProps(ex));
            sw->propMeta = std::move(meta);
            applyBindings(sw.get(), pv);
            return sw;
        });

        // // ── Line ──
        // ElementParser::registerType("Line", [](const JSValueRef &pv) {
        //     TypedPropMap meta;
        //     PropsExtractor ex(pv, &meta);
        //     return std::make_unique<Line>(parseViewProps(ex), parseLineProps(ex));
        // });

        // // ── Spinner ──
        // ElementParser::registerType("Spinner", [](const JSValueRef &pv) {
        //     TypedPropMap meta;
        //     PropsExtractor ex(pv, &meta);
        //     return std::make_unique<Spinner>(parseViewProps(ex), parseSpinnerProps(ex));
        // });

        // // ── Table ──
        // ElementParser::registerType("Table", [](const JSValueRef &pv) {
        //     TypedPropMap meta;
        //     PropsExtractor ex(pv, &meta);
        //     auto table = std::make_unique<Table>(parseViewProps(ex), parseTableProps(ex));

        //     // 保留 data 数组的 JS 引用
        //     if (pv.hasProperty("data")) {
        //         auto dataVal = pv.getProperty("data");
        //         if (dataVal.isArray()) {
        //             JSContext *ctx = dataVal.context();
        //             table->setJSData(ctx, JS_DupValue(ctx, dataVal.raw()));
        //         }
        //     }

        //     return table;
        // });

        // // ── TextView（富文本编辑器）──
        // ElementParser::registerType("TextView", [](const JSValueRef &pv) {
        //     TypedPropMap meta;
        //     PropsExtractor ex(pv, &meta);
        //     auto tv = std::make_unique<TextView>(parseViewProps(ex), parseTextViewProps(ex));
        //     tv->propMeta = std::move(meta);
        //     applyBindings(tv.get(), pv);
        //     Log::debug("TextView created: id={}", tv->getProperty("id"));
        //     return tv;
        // });
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
    if (type.empty()) {
        Log::error("parseNode: 缺少 type 字段 — request import type");
        return nullptr;
    }
    // ── 2. 获取 props JS 对象 ─────────────────────────────────────
    auto propsVal = jsVal.getProperty("props");

    // ── 3. 查注册表分发创建 C++ 组件 ──────────────────────────
    std::unique_ptr<View> element;
    auto registry = creators();
    auto it = registry.find(type);
    if (it != registry.end()) {
        element = it->second(propsVal);
        if (!element) { Log::error("parseNode: 组件 '{}' 工厂函数返回 null", type); }
    } else {
        Log::warn("parseNode: 未注册的类型 '{}' — 降级为 View", type);
        TypedPropMap meta;
        PropsExtractor ex(propsVal, &meta);
        element = std::make_unique<View>(parseViewProps(ex));
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
        tryBind("onChange");
        tryBind("onRowClick");
    }
    
    return element;
}

// ============================================================================
// 公开接口 - printTree 打印完整 View 组件树
// ============================================================================
void ElementParser::printTree(const View *view, int depth, const std::string &prefix) {
    if (!view) return;
    const auto &p = view->props;
    std::print("{}{} [{:.0f}x{:.0f}]", prefix, to_string(view->type()), view->frame.width, view->frame.height);
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