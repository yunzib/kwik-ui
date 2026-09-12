module;

module kwik.core.prop_meta;

import kwik.core.types;
import kwik.core.props;

import std;

// ═══════════════════════════════════════════════════════════════════════════
// 属性描述符注册表 — 按 PropId 顺序排列，一一对应枚举值
// ═══════════════════════════════════════════════════════════════════════════

static const PropMeta kPropMetas[] = {
    // ── 显示属性 ──
    [static_cast<int>(PropId::opacity)] = {
        PropId::opacity, "opacity", nullptr, PropUnit::Ratio, PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.opacity);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.opacity = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::scale)] = {
        PropId::scale, "scale", nullptr, PropUnit::Ratio, PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.transform ? p.transform->scale : 1.0f);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            if (!p.transform) p.transform = Transform{};
            p.transform->scale = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::visible)] = {
        PropId::visible, "visible", nullptr, PropUnit::None, PropFlags::None, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return p.visible;
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.visible = std::get<bool>(v);
        },
    },
    [static_cast<int>(PropId::background)] = {
        PropId::background, "background", "bg,backgroundColor", PropUnit::None, PropFlags::Anim, true,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return p.background;
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.background = std::get<Color>(v);
        },
    },
    [static_cast<int>(PropId::borderRadius)] = {
        PropId::borderRadius, "borderRadius", "radius", PropUnit::Px, PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.borderRadius);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.borderRadius = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::borderWidth)] = {
        PropId::borderWidth, "borderWidth", "bw", PropUnit::Px, PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.borderWidth);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.borderWidth = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::borderColor)] = {
        PropId::borderColor, "borderColor", "bc", PropUnit::None, PropFlags::Anim, true,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return p.borderColor;
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.borderColor = std::get<Color>(v);
        },
    },
    [static_cast<int>(PropId::shadow)] = {
        PropId::shadow, "shadow", nullptr, PropUnit::Px, PropFlags::None, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            // 仅支持 flip（t >= 0.5 切换），不支持 tween
            return std::monostate{};
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            // shadow 暂不通过动画驱动
        },
    },

    // ── 变换 ──
    [static_cast<int>(PropId::transform)] = {
        PropId::transform, "transform", nullptr, PropUnit::None, PropFlags::None, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return p.transform.value_or(Transform{});
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.transform = std::get<Transform>(v);
        },
    },
    [static_cast<int>(PropId::translateX)] = {
        PropId::translateX, "translateX", "tx", PropUnit::Px, PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(
                p.transform ? p.transform->translateX : 0.0f);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            if (!p.transform) p.transform = Transform{};
            p.transform->translateX = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::translateY)] = {
        PropId::translateY, "translateY", "ty", PropUnit::Px, PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(
                p.transform ? p.transform->translateY : 0.0f);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            if (!p.transform) p.transform = Transform{};
            p.transform->translateY = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::rotate)] = {
        PropId::rotate, "rotate", nullptr, PropUnit::Deg, PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.transform ? p.transform->rotate : 0.0f);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            if (!p.transform) p.transform = Transform{};
            p.transform->rotate = static_cast<float>(std::get<double>(v));
        },
    },

    // ── 尺寸（变化后触发 re-layout）──
    [static_cast<int>(PropId::width)] = {
        PropId::width, "width", "w", PropUnit::Px, PropFlags::Layout | PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.width.value_or(0.0f));
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.width = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::height)] = {
        PropId::height, "height", "h", PropUnit::Px, PropFlags::Layout | PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.height.value_or(0.0f));
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.height = static_cast<float>(std::get<double>(v));
        },
    },

    // ── 间距（变化后触发 re-layout）──
    [static_cast<int>(PropId::padding)] = {
        PropId::padding, "padding", nullptr, PropUnit::Px, PropFlags::Layout | PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return p.padding;
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.padding = std::get<EdgeInsets>(v);
        },
    },
    [static_cast<int>(PropId::margin)] = {
        PropId::margin, "margin", nullptr, PropUnit::Px, PropFlags::Layout | PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return p.margin;
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.margin = std::get<EdgeInsets>(v);
        },
    },

    // ── 位置（绝对定位，不影响兄弟节点布局，但自身 frame 需重排生效）──
    // Layout 标志：setProperty/动画改 x/y 须 requestLayout 重排，
    // 否则仅改 props.x 而 frame 不更新（视图不动）
    [static_cast<int>(PropId::x)] = {
        PropId::x, "x", nullptr, PropUnit::Px, PropFlags::Layout | PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.x);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.x = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::y)] = {
        PropId::y, "y", nullptr, PropUnit::Px, PropFlags::Layout | PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.y);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.y = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::absTop)] = {
        PropId::absTop, "absTop", nullptr, PropUnit::Px, PropFlags::Layout | PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.absTop);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.absTop = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::absLeft)] = {
        PropId::absLeft, "absLeft", nullptr, PropUnit::Px, PropFlags::Layout | PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.absLeft);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.absLeft = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::absRight)] = {
        PropId::absRight, "absRight", nullptr, PropUnit::Px, PropFlags::Layout | PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.absRight);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.absRight = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::absBottom)] = {
        PropId::absBottom, "absBottom", nullptr, PropUnit::Px, PropFlags::Layout | PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.absBottom);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.absBottom = static_cast<float>(std::get<double>(v));
        },
    },

    // ── 文字 ──
    // textColor/fontSize 属 TextContent（Text 组件持有），基类条目为桩——
    // 动画路径经子类覆写处理（后续修复件中归位 Text 覆写）
    [static_cast<int>(PropId::textColor)] = {
        PropId::textColor, "textColor", nullptr, PropUnit::None, PropFlags::Anim, true,
        /*reader*/ [](const ViewProps&) -> TypedProp {
            return Color{0, 0, 0, 255};
        },
        /*writer*/ [](ViewProps&, const TypedProp&) {
            // 子类覆盖
        },
    },
    [static_cast<int>(PropId::fontSize)] = {
        PropId::fontSize, "fontSize", nullptr, PropUnit::Px, PropFlags::Anim, false,
        /*reader*/ [](const ViewProps&) -> TypedProp {
            return 0.0; // 子类覆盖
        },
        /*writer*/ [](ViewProps&, const TypedProp&) {
            // 子类覆盖
        },
    },
    [static_cast<int>(PropId::backdropBlur)] = {
        PropId::backdropBlur, "backdropBlur", nullptr, PropUnit::Px, PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.backdropBlur);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.backdropBlur = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::backdropRefraction)] = {
        PropId::backdropRefraction, "backdropRefraction", nullptr, PropUnit::Px, PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.backdropRefraction);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.backdropRefraction = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::backdropSpecular)] = {
        PropId::backdropSpecular, "backdropSpecular", nullptr, PropUnit::Ratio, PropFlags::Anim, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.backdropSpecular);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.backdropSpecular = static_cast<float>(std::get<double>(v));
        },
    },
};

// sentinel 校验
static_assert(
    static_cast<int>(PropId::COUNT) == sizeof(kPropMetas) / sizeof(PropMeta),
    "kPropMetas size must match PropId::COUNT");

const PropMeta& getPropMeta(PropId id) {
    auto idx = static_cast<int>(id);
    if (idx < 0 || idx >= static_cast<int>(PropId::COUNT)) {
        // 返回一个空元数据作为安全 fallback
        static const PropMeta empty{PropId::COUNT, nullptr, nullptr, PropUnit::None,
                                    PropFlags::None, false, nullptr, nullptr};
        return empty;
    }
    return kPropMetas[idx];
}

// ═══════════════════════════════════════════════════════════════════════════
// 属性名 ↔ PropId 双向转换（查 kPropMetas，名字/别名已并入条目，无独立名单）
// ═══════════════════════════════════════════════════════════════════════════

// 逗号分隔别名表匹配："bg,backgroundColor" 含 name 即命中
static bool matchAlias(const char *aliases, std::string_view name) {
    if (!aliases) return false;
    std::string_view csv{aliases};
    auto pos = std::string_view::size_type{0};
    while (pos <= csv.size()) {
        auto comma = csv.find(',', pos);
        if (comma == std::string_view::npos) comma = csv.size();
        if (csv.substr(pos, comma - pos) == name) return true;
        pos = comma + 1;
    }
    return false;
}

PropId propIdFromName(std::string_view name) {
    for (const auto &m : kPropMetas) {                 // 正名优先（规范名不含歧义）
        if (name == m.name) return m.id;
    }
    for (const auto &m : kPropMetas) {                 // 别名兜底
        if (matchAlias(m.aliases, name)) return m.id;
    }
    return PropId::COUNT;
}

const char* propName(PropId id) {
    auto idx = static_cast<int>(id);
    if (idx < 0 || idx >= static_cast<int>(PropId::COUNT)) return "?";
    return kPropMetas[idx].name;
}