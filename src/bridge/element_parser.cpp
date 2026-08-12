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
import kwik.bridge.event_adapter; // attachJsHandlers — JS 事件适配
import kwik.element.slider;
import kwik.element.progressbar;
import kwik.element.switch_button;
import kwik.element.line;
import kwik.element.spinner;
import kwik.element.table;
import kwik.element.textview;
import kwik.element.text;
import kwik.element.button;
import kwik.element.input;
import kwik.element.radiobutton;
import kwik.layout.radio_group;
import kwik.element.checkbox;
import kwik.element.textarea;
import kwik.element.dropdown;
import kwik.element.tabs;
import kwik.element.g2d;
import kwik.core.theme;                  // ThemeData — 主题数据结构
import kwik.bridge.theme_bridge;         // unwrapThemeData — JS opaque → C++ ThemeData*
import kwik.element.theme_provider;      // ThemeProvider — 主题注入 View 节点
import kwik.bridge.js_table_data_source; // createJsTableDataSource — 注入 Table 数据源
import kwik.element.stack_index;
import kwik.element.layer_view;
import kwik.element.g3d;
import kwik.element.scroll_view;
import kwik.element.tree_menu;
import kwik.element.lazy_list; // LazyList — 虚拟化列表
import kwik.element.lazy_list_source; 

import std;

/**
 * @brief 统一绑定注入 — 遍历 propMeta 中 hasBinding=true 的属性，
 *        逐一建立双向绑定链路：
 *
 *   State → View（增量更新）: 注册到 BindingRegistry，
 *       后续 state.key 变更直接调用 setPropertyTyped + markDirty，
 *       跳过 rebuildTree。
 *
 *   View → State（背向传播）: 调用 View::setBinding()。
 *       基类默认空实现；Input/Checkbox/Dropdown 等交互组件覆写此虚函数，
 *       将 View 属性变更回写到 State。
 *
 * @param view  任意 View 子类（原模板仅覆盖 Input 等 9 种交互组件，
 *              去模板化后覆盖全部 26 种组件类型）
 * @param pv    JS props 对象（含 __bind_*State / __bind_*Key 隐藏属性）
 */
static void applyBindings(View *view, const JSValueRef &pv) {
    auto &meta = view->propMeta;
    JSContext *ctx = pv.context();

    meta.forEachBinding([&](const std::string &propName, const PropEntry &) {
        std::string stateName = "__bind_" + propName + "State";
        std::string keyName = "__bind_" + propName + "Key";
        auto stateVal = pv.getProperty(stateName.c_str());
        auto keyVal = pv.getProperty(keyName.c_str());

        if (!stateVal.isUndefined() && !keyVal.isUndefined() && !JS_IsNull(stateVal.raw())) {
            // ── View → State 反向绑定（基类默认空实现，交互组件覆写）──
            view->setBinding(createJSBinding(ctx, stateVal.raw()), keyVal.toString());

            // ── State → View 增量绑定（通用，所有组件受益）──
            if (auto *reg = getRegisteredRegistry()) {
                void *statePtr = JS_VALUE_GET_PTR(stateVal.raw());
                reg->bind(statePtr, keyVal.toString(), view, propName);
            }

            // Dropdown 特殊处理：ref 已将 value 替换为 State 当前值，
            // 但 Dropdown 需将字符串值映射为 selectedIndex
            if (view->type() == ElementType::Dropdown && pv.hasProperty("value")) {
                std::string val = pv.getProperty("value").toString();
                if (!val.empty()) view->setProperty("value", val.c_str());
            }
        }
    });
}

/**
 * @brief 从 JS props 提取 @ token 写入 View
 *
 * 遍历 props 对象所有可枚举字符串属性，若值以 '@' 开头，
 * 将 {属性名 → 去 @ 后的 token 名} 写入 view->props.themeTokens。
 *
 * 动机：TypeCreator 中 parseViewProps(ex) 在组件专有解析函数
 * (parseTextContent / parseInputProps 等) 之前被求值，导致
 * 后者收集的 @ token 无法传入 ViewProps。本函数作为统一补救，
 * 在 TypeCreator 返回后重新扫描 JS props，保证所有 @ token 被捕获。
 *
 * @param view     目标 View（themeTokens 将被填充）
 * @param propsVal JS props 对象（与传递给 TypeCreator 的同一对象）
 */
static void extractThemeTokens(View *view, const JSValueRef &propsVal) {
    if (!view || !propsVal.isObject()) return;
    JSContext *ctx = propsVal.context();
    JSPropertyEnum *tab;
    uint32_t len;
    JSValue props = propsVal.raw();
    if (JS_GetOwnPropertyNames(ctx, &tab, &len, props, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) != 0) return;
    for (uint32_t i = 0; i < len; ++i) {
        const char *name = JS_AtomToCString(ctx, tab[i].atom);
        if (name) {
            JSValue val = JS_GetProperty(ctx, props, tab[i].atom);
            if (JS_IsString(val)) {
                const char *str = JS_ToCString(ctx, val);
                if (str && str[0] == '@') view->props.themeTokens[name] = std::string(str + 1);
                if (str) JS_FreeCString(ctx, str);
            }
            JS_FreeValue(ctx, val);
            JS_FreeCString(ctx, name);
        }
        JS_FreeAtom(ctx, tab[i].atom);
    }
    js_free(ctx, tab);
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
            auto v = std::make_unique<View>(parseViewProps(ex));
            v->propMeta = std::move(meta);    // 保存绑定元数据（hasBinding 标记等）
            applyBindings(v.get(), pv);       // 注册到 BindingRegistry（State→View 增量）
            return v;
        });

        ElementParser::registerType("Root", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto v = std::make_unique<RootView>(parseViewProps(ex));
            v->propMeta = std::move(meta);
            applyBindings(v.get(), pv);
            return v;
        });

        ElementParser::registerType("Text", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto v = std::make_unique<Text>(parseViewProps(ex), parseTextContent(ex));
            v->propMeta = std::move(meta);    // 保存绑定元数据（hasBinding 标记等）
            applyBindings(v.get(), pv);       // 注册到 BindingRegistry（State→View 增量）
            return v;
        });

        ElementParser::registerType("Button", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto v = std::make_unique<Button>(parseViewProps(ex), parseTextContent(ex), parseButtonState(ex));
            v->propMeta = std::move(meta);    // 保存绑定元数据（hasBinding 标记等）
            applyBindings(v.get(), pv);       // 注册到 BindingRegistry（State→View 增量）
            return v;
        });

        ElementParser::registerType("Flex", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto v = std::make_unique<FlexLayout>(parseViewProps(ex), parseContainerProps(ex));
            v->propMeta = std::move(meta);    // 保存绑定元数据（hasBinding 标记等）
            applyBindings(v.get(), pv);       // 注册到 BindingRegistry（State→View 增量）
            return v;
        });

        ElementParser::registerType("Grid", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto v = std::make_unique<GridLayout>(parseViewProps(ex), parseContainerProps(ex));
            v->propMeta = std::move(meta);    // 保存绑定元数据（hasBinding 标记等）
            applyBindings(v.get(), pv);       // 注册到 BindingRegistry（State→View 增量）
            return v;
        });

        ElementParser::registerType("Stack", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto v = std::make_unique<StackLayout>(parseViewProps(ex));
            v->propMeta = std::move(meta);    // 保存绑定元数据（hasBinding 标记等）
            applyBindings(v.get(), pv);       // 注册到 BindingRegistry（State→View 增量）
            return v;
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
            list->propMeta = std::move(meta);
            applyBindings(list.get(), pv);
            return list;
        });

        ElementParser::registerType("Image", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto v = std::make_unique<Image>(parseViewProps(ex), parseImageProps(ex));
            v->propMeta = std::move(meta);
            applyBindings(v.get(), pv);
            return v;
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
            auto v = std::make_unique<RadioButton>(parseViewProps(ex), parseTextContent(ex), parseRadioButtonProps(ex));
            v->propMeta = std::move(meta);
            applyBindings(v.get(), pv);
            return v;
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
            // Log::debug("Checkbox created: id={}", checkbox->getProperty("id"));
            return checkbox;
        });

        // ── TextArea ──
        ElementParser::registerType("TextArea", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto ta = std::make_unique<TextArea>(parseViewProps(ex), parseTextAreaProps(ex));
            ta->propMeta = std::move(meta);
            applyBindings(ta.get(), pv);
            return ta;
        });

        // ── Dropdown ──
        ElementParser::registerType("Dropdown", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto dd = std::make_unique<Dropdown>(parseViewProps(ex), parseDropdownProps(ex));
            dd->propMeta = std::move(meta);
            applyBindings(dd.get(), pv);
            return dd;
        });

        // ── Slider ──
        ElementParser::registerType("Slider", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto slider = std::make_unique<Slider>(parseViewProps(ex), parseSliderProps(ex));
            slider->propMeta = std::move(meta);
            applyBindings(slider.get(), pv);
            return slider;
        });

        // ── ProgressBar ──
        ElementParser::registerType("ProgressBar", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto pb = std::make_unique<ProgressBar>(parseViewProps(ex), parseProgressBarProps(ex));
            pb->propMeta = std::move(meta);
            applyBindings(pb.get(), pv);
            return pb;
        });

        // ── Switch ──
        ElementParser::registerType("Switch", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto sw = std::make_unique<Switch>(parseViewProps(ex), parseSwitchProps(ex));
            sw->propMeta = std::move(meta);
            applyBindings(sw.get(), pv);
            return sw;
        });

        // ── Line ──
        ElementParser::registerType("Line", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto v = std::make_unique<Line>(parseViewProps(ex), parseLineProps(ex));
            v->propMeta = std::move(meta);
            applyBindings(v.get(), pv);
            return v;
        });

        // ── Spinner ──
        ElementParser::registerType("Spinner", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto v = std::make_unique<Spinner>(parseViewProps(ex), parseSpinnerProps(ex));
            v->propMeta = std::move(meta);
            applyBindings(v.get(), pv);
            return v;
        });

        // ── Table ──
        ElementParser::registerType("Table", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto table = std::make_unique<Table>(parseViewProps(ex), parseTableProps(ex));

            // 注入数据源 (JS 数组 → JsTableDataSource, 内部 Dup 持有)
            if (pv.hasProperty("data")) {
                auto dataVal = pv.getProperty("data");
                if (dataVal.isArray()) { table->setData(createJsTableDataSource(dataVal.context(), dataVal.raw())); }
            }

            table->propMeta = std::move(meta);    // ← 新增（在 data引用保留之后）
            applyBindings(table.get(), pv);       // ← 新增
            return table;
        });

        // ── TextView（富文本编辑器）──
        ElementParser::registerType("TextView", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto tv = std::make_unique<TextView>(parseViewProps(ex), parseTextViewProps(ex));
            tv->propMeta = std::move(meta);
            applyBindings(tv.get(), pv);
            Log::debug("TextView created: id={}", tv->getProperty("id"));
            return tv;
        });

        // ── Tabs ──
        ElementParser::registerType("Tabs", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto tabs = std::make_unique<Tabs>(parseViewProps(ex), parseTabsProps(ex));
            tabs->propMeta = std::move(meta);
            applyBindings(tabs.get(), pv);
            return tabs;
        });

        ElementParser::registerType("G2D", [](const JSValueRef &pv) {
            // 检查是否已有 eager 创建的 C++ G2D
            if (pv.hasProperty("__g2d_ptr")) {
                auto ptrVal = pv.getProperty("__g2d_ptr");
                JSContext *ctx = pv.context();
                double v;
                JS_ToFloat64(ctx, &v, ptrVal.raw());
                auto *existing = reinterpret_cast<G2D *>(static_cast<uintptr_t>(v));
                // 从 JS props 更新 ViewProps（width/height 等）
                TypedPropMap meta;
                PropsExtractor ex(pv, &meta);
                existing->props = parseViewProps(ex);
                return std::unique_ptr<G2D>(existing);    // 树接管所有权
            }
            // 降级：正常创建（无 eager 创建的场景）
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto v = std::make_unique<G2D>(parseViewProps(ex));
            v->propMeta = std::move(meta);
            applyBindings(v.get(), pv);
            return v;
        });

        // ── ThemeProvider — 主题注入节点 ──
        // JS 侧: 用户在 Root/View 中声明 theme: theme({colors:{primary:"#1976D2"}})
        // C++ 侧: 解析为 ThemeProvider View, 子树内的组件通过 View::theme() 获取
        ElementParser::registerType("ThemeProvider", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            // 从 JS props 的 "theme" 字段提取 opaque ThemeData 指针
            ThemeData data = ThemeData::defaultTheme();
            if (ex.has("theme")) {
                JSValue themeVal = ex.raw().getProperty("theme").raw();
                const ThemeData *ptr = unwrapThemeData(themeVal);
                if (ptr) data = *ptr;    // 复制堆上的 ThemeData
            }
            auto tp = std::make_unique<ThemeProvider>(parseViewProps(ex), std::move(data));
            tp->propMeta = std::move(meta);
            applyBindings(tp.get(), pv);
            return tp;
        });

        ElementParser::registerType("StackIndex", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto v = std::make_unique<StackIndex>(parseViewProps(ex), parseStackIndexProps(ex));
            v->propMeta = std::move(meta);
            applyBindings(v.get(), pv);
            return v;
        });

        // ── Layer — 通用浮层原语（M2）──
        ElementParser::registerType("Layer", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto v = std::make_unique<LayerView>(parseViewProps(ex), parseLayerProps(ex));
            v->propMeta = std::move(meta);
            applyBindings(v.get(), pv);
            return v;
        });

        ElementParser::registerType("G3D", [](const JSValueRef &pv) {
            // 检查是否已有 eager 创建的 C++ G3D (js_g3d 工厂已构造)
            if (pv.hasProperty("__g3d_ptr")) {
                auto ptrVal = pv.getProperty("__g3d_ptr");
                JSContext *ctx = pv.context();
                double v;
                JS_ToFloat64(ctx, &v, ptrVal.raw());
                auto *existing = reinterpret_cast<G3D *>(static_cast<uintptr_t>(v));
                // 从 JS props 更新 ViewProps (width/height 等)
                TypedPropMap meta;
                PropsExtractor ex(pv, &meta);
                existing->props = parseViewProps(ex);
                return std::unique_ptr<G3D>(existing);    // 树接管所有权
            }
            // 降级：正常创建（无 eager 创建的场景）
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto v = std::make_unique<G3D>(parseViewProps(ex));
            v->propMeta = std::move(meta);
            applyBindings(v.get(), pv);
            return v;
        });

        // ── ScrollView ──
        ElementParser::registerType("ScrollView", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto v = std::make_unique<ScrollView>(parseViewProps(ex), parseScrollViewProps(ex));
            v->propMeta = std::move(meta);
            applyBindings(v.get(), pv);
            return v;
        });

        // ── TreeMenu ──
        ElementParser::registerType("TreeMenu", [](const JSValueRef &pv) {
            PropsExtractor ex(pv);
            auto v = std::make_unique<TreeMenu>(parseViewProps(ex), parseScrollViewProps(ex), parseTreeMenuProps(ex));
            return v;
        });

        ElementParser::registerType("LazyList", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto list =
                std::make_unique<LazyList>(parseViewProps(ex), parseScrollViewProps(ex), parseLazyListProps(ex));

            // header / footer（私有成员，解析路径与 List 的 header/footer 同构）
            if (pv.hasProperty("header") && pv.getProperty("header").isObject()) {
                auto hdr = pv.getProperty("header");
                JSValueRef node(hdr.context(), JS_DupValue(hdr.context(), hdr.raw()));
                list->setHeader(ElementParser::parseNode(node));
            }
            if (pv.hasProperty("footer") && pv.getProperty("footer").isObject()) {
                auto ftr = pv.getProperty("footer");
                JSValueRef node(ftr.context(), JS_DupValue(ftr.context(), ftr.raw()));
                list->setFooter(ElementParser::parseNode(node));
            }

            // 数据源：items 数组 + itemBuilder 函数 → 经钩子创建的 bridge 数据源
            const auto &fac = lazyListSourceFactory();
            if (fac && pv.hasProperty("items") && pv.getProperty("items").isArray()) {
                auto itemsVal = pv.getProperty("items");
                JSValue bv = pv.hasProperty("itemBuilder") ? pv.getProperty("itemBuilder").raw() : JS_UNDEFINED;
                list->setDataSource(fac(itemsVal.context(), itemsVal.raw(), bv));
            }

            list->propMeta = std::move(meta);
            applyBindings(list.get(), pv);
            return list;
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

    // ── 提取 @ token ──
    extractThemeTokens(element.get(), propsVal);

    // ── 4. 递归解析子节点 ────────────────────────────────────────────
    auto childrenVal = jsVal.getProperty("children");
    if (childrenVal.isArray()) {
        int len = childrenVal.getArrayLength();
        for (int i = 0; i < len; ++i) {
            auto child = parseNode(childrenVal.getArrayElement(i));
            if (child) {
                element->addChild(std::move(child));
                element->children.back()->resolveThemeDefaults();
            }
        }
    }
    // ── 5. 提取事件处理器 (JS 回调 → std::function 适配, 见 event_adapter) ──
    if (element && propsVal.isObject()) { attachJsHandlers(*element, propsVal); }

    return element;
}

/** @brief 重新绑定事件处理器（reconcile 复用旧 View 后调用）
 *
 *  覆盖式重绑: 旧 std::function 析构时自动释放其持有的 JSValue,
 *  * 覆盖式重绑: 旧 std::function 析构自动释放其持有的 JSValue。
 */
void ElementParser::rebindHandlers(View *view, const JSValueRef &propsVal) {
    if (view && propsVal.isObject()) { attachJsHandlers(*view, propsVal); }
}

/**
 * @brief 按 id + 位置对齐新旧子节点列表
 *
 * ① 扫描旧 children，将带 id 的节点建 map（id→索引）
 * ② 遍历新 JS children：
 *    - id 命中 → 取出旧节点 → reconcileNode 复用
 *    - id 未命中 → 按遍历顺序取下一个未被认领的旧节点（位置匹配）
 *    - 无可用旧节点 → parseNode 新建
 * ③ 剩余未被认领的旧节点 → 解绑 BindingRegistry → 析构
 */
void ElementParser::reconcileChildren(View *parent, const JSValueRef &childrenVal,
                                      std::vector<std::unique_ptr<View>> &oldChildren) {
    if (!childrenVal.isArray()) return;

    size_t oldN = oldChildren.size();

    // ── ① 建 id→索引 映射 ──
    std::unordered_map<std::string, size_t> oldById;
    for (size_t i = 0; i < oldN; ++i) {
        if (!oldChildren[i]->props.id.empty()) oldById[oldChildren[i]->props.id] = i;
    }

    // ── ② 逐个新节点匹配 ──
    std::vector<bool> claimed(oldN, false);
    std::vector<std::unique_ptr<View>> newChildren;
    int newLen = childrenVal.getArrayLength();
    size_t posCursor = 0;    // 位置匹配游标（遍历 oldN，跳过已被 id 认领的）

    for (int i = 0; i < newLen; ++i) {
        auto jsChild = childrenVal.getArrayElement(i);
        std::string jsType = jsChild.getProperty("type").toString();
        auto jsProps = jsChild.getProperty("props");
        std::string jsId;

        // 尝试读 id
        if (jsProps.isObject() && jsProps.hasProperty("id")) jsId = jsProps.getProperty("id").toString();

        size_t matchIdx = SIZE_MAX;

        // ── id 优先匹配 ──
        if (!jsId.empty()) {
            auto it = oldById.find(jsId);
            if (it != oldById.end() && !claimed[it->second]) {
                ElementType oldType = oldChildren[it->second]->type();
                ElementType newType = elementTypeFromString(jsType);
                if (oldType == newType) { matchIdx = it->second; }
            }
        }

        // ── id 未命中 → 位置+类型匹配 ──
        if (matchIdx == SIZE_MAX) {
            ElementType newType = elementTypeFromString(jsType);
            for (; posCursor < oldN; ++posCursor) {
                if (claimed[posCursor]) continue;
                ElementType oldType = oldChildren[posCursor]->type();
                if (oldType == newType) {
                    matchIdx = posCursor;
                    ++posCursor;
                    break;
                }
            }
        }

        // ── 复用 or 新建 ──
        if (matchIdx != SIZE_MAX) {
            claimed[matchIdx] = true;
            auto oldView = std::move(oldChildren[matchIdx]);
            newChildren.push_back(reconcileNode(jsChild, std::move(oldView)));
        } else {
            newChildren.push_back(parseNode(jsChild));
        }
    }

    // ── ③ 清理未被认领的旧节点 ──
    auto *reg = getRegisteredRegistry();
    for (size_t i = 0; i < oldN; ++i) {
        if (!claimed[i] && oldChildren[i]) {
            if (reg) reg->unbind(oldChildren[i].get());
            // unique_ptr 在此析构 → 递归 ~View()
        }
    }

    // 替换 parent 的 children
    parent->children.clear();    // View::children 是 public vector<unique_ptr<View>>
    for (auto &child : newChildren) parent->addChild(std::move(child));
}

/**
 * @brief 尝试复用旧 View
 *
 * 若 jsVal.type 与 oldView.type() 一致 → 原地更新 props + propMeta + handlers，
 * 并在 reconcileChildren 中递归处理子节点。
 * 若类型不一致 → 解绑旧 View + parseNode 创建新 View。
 * oldView 为 nullptr 时走 parseNode 创建。
 */
std::unique_ptr<View> ElementParser::reconcileNode(const JSValueRef &jsVal, std::unique_ptr<View> oldView) {
    if (!oldView) return parseNode(jsVal);

    // ── 读 JS 节点信息 ──
    auto typeVal = jsVal.getProperty("type");
    std::string jsType = typeVal.toString();
    auto propsVal = jsVal.getProperty("props");

    ElementType newType = elementTypeFromString(jsType);
    ElementType oldType = oldView->type();

    // ── 类型不一致 → 销毁旧 View，创建新 View ──
    if (newType != oldType) {
        if (auto *reg = getRegisteredRegistry()) reg->unbind(oldView.get());
        return parseNode(jsVal);
    }

    // ── 类型一致 → 原地更新 props ──
    TypedPropMap meta;
    PropsExtractor ex(propsVal, &meta);
    oldView->props = parseViewProps(ex);    // ← ViewProps 字段级覆盖

    extractThemeTokens(oldView.get(), propsVal);

    // ── 组件专有属性解析（text_ 已 public，直接赋值，复用 TypeCreator 同源解析函数）──
    switch (oldView->type()) {
    case ElementType::Text: static_cast<Text *>(oldView.get())->text_ = parseTextContent(ex); break;
    case ElementType::Button:
        static_cast<Button *>(oldView.get())->text_ = parseTextContent(ex);
        static_cast<Button *>(oldView.get())->button_ = parseButtonState(ex);
        break;
    case ElementType::LayerView: static_cast<LayerView *>(oldView.get())->applyLayerProps(parseLayerProps(ex)); break;
    case ElementType::ScrollView:
        static_cast<ScrollView *>(oldView.get())->applyScrollProps(parseScrollViewProps(ex));
        break;
    case ElementType::TreeMenu: {
        auto *tm = static_cast<TreeMenu *>(oldView.get());
        tm->applyScrollProps(parseScrollViewProps(ex));
        tm->applyTreeMenuProps(parseTreeMenuProps(ex));
        break;
    }
    case ElementType::LazyList: {
        auto *ll = static_cast<LazyList *>(oldView.get());
        ll->applyScrollProps(parseScrollViewProps(ex));    // direction 等（旧 applyScrollProps 路径）
        ll->applyLazyListProps(parseLazyListProps(ex));    // 行高/估计/overscan/分割线 → 清窗重建

        // header/footer 重建（reconcile 不递归进私有成员，直接重建最简）
        // 旧 header/footer 销毁前先解绑根节点（与 reconcile 其余路径的 unbind 语义一致）
        if (propsVal.hasProperty("header") && propsVal.getProperty("header").isObject()) {
            if (auto old = ll->takeHeader()) {
    if (auto *reg = getRegisteredRegistry()) reg->unbind(old.get());
}
            auto hdr = propsVal.getProperty("header");
            JSValueRef node(hdr.context(), JS_DupValue(hdr.context(), hdr.raw()));
            ll->setHeader(ElementParser::parseNode(node));
        }
        if (propsVal.hasProperty("footer") && propsVal.getProperty("footer").isObject()) {
            if (auto old = ll->takeFooter()) {
                if (auto *reg = getRegisteredRegistry()) reg->unbind(old.get());
            }
            auto ftr = propsVal.getProperty("footer");
            JSValueRef node(ftr.context(), JS_DupValue(ftr.context(), ftr.raw()));
            ll->setFooter(ElementParser::parseNode(node));
        }

        // 数据源重建（items/itemBuilder 变更 → applyLazyListProps 已清窗，
        // setDataSource 再以新数据源重出窗）
        const auto &fac = lazyListSourceFactory();
        if (fac && propsVal.hasProperty("items") && propsVal.getProperty("items").isArray()) {
            auto itemsVal = propsVal.getProperty("items");
            JSValue bv = propsVal.hasProperty("itemBuilder") ? propsVal.getProperty("itemBuilder").raw() : JS_UNDEFINED;
            ll->setDataSource(fac(itemsVal.context(), itemsVal.raw(), bv));
        }
        break;
    }
    default: break;
    }

    oldView->propMeta = std::move(meta);       // ← 更新 hasBinding 标记
    applyBindings(oldView.get(), propsVal);    // ← 重新注册到 BindingRegistry

    // ── 递归 reconcile children ──
    auto childrenVal = jsVal.getProperty("children");
    // LazyList 的 children 全是虚拟行（LazyListRow），绝不能被 JS children 替换/冲掉
    if (childrenVal.isArray() && oldView->type() != ElementType::LazyList) {
        reconcileChildren(oldView.get(), childrenVal, oldView->children);
    }

    // ── 重新绑定事件处理器 ──
    rebindHandlers(oldView.get(), propsVal);

    return oldView;
}

std::unique_ptr<View> ElementParser::reconcile(JSContext *ctx, JSValueConst value, std::unique_ptr<View> oldRoot) {
    JSValueRef jsVal(ctx, JS_DupValue(ctx, value));
    if (!jsVal.isObject() || jsVal.isNull()) return std::move(oldRoot);
    return reconcileNode(jsVal, std::move(oldRoot));
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

// ════════════════════════════════════════════════════════
// LazyList 数据源工厂钩子实现（见 element_parser.cppm 声明）
// ════════════════════════════════════════════════════════
namespace {
LazyListSourceFactory &lazyFactory() {
    static LazyListSourceFactory f;    // 函数局部静态：注册/读取共享，无初始化顺序问题
    return f;
}
}    // namespace

void registerLazyListSourceFactory(LazyListSourceFactory f) {
    lazyFactory() = std::move(f);
}
const LazyListSourceFactory &lazyListSourceFactory() {
    return lazyFactory();
}