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
        PropId::opacity, false, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.opacity);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.opacity = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::scale)] = {
        PropId::scale, false, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.transform ? p.transform->scale : 1.0f);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            if (!p.transform) p.transform = Transform{};
            p.transform->scale = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::visible)] = {
        PropId::visible, false, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return p.visible;
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.visible = std::get<bool>(v);
        },
    },
    [static_cast<int>(PropId::background)] = {
        PropId::background, false, true,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return p.background;
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.background = std::get<Color>(v);
        },
    },
    [static_cast<int>(PropId::borderRadius)] = {
        PropId::borderRadius, false, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.borderRadius);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.borderRadius = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::borderWidth)] = {
        PropId::borderWidth, false, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.borderWidth);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.borderWidth = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::borderColor)] = {
        PropId::borderColor, false, true,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return p.borderColor;
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.borderColor = std::get<Color>(v);
        },
    },
    [static_cast<int>(PropId::shadow)] = {
        PropId::shadow, false, false,
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
        PropId::transform, false, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return p.transform.value_or(Transform{});
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.transform = std::get<Transform>(v);
        },
    },
    [static_cast<int>(PropId::translateX)] = {
        PropId::translateX, false, false,
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
        PropId::translateY, false, false,
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
        PropId::rotate, false, false,
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
        PropId::width, /*layoutAffecting*/ true, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.width.value_or(0.0f));
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.width = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::height)] = {
        PropId::height, /*layoutAffecting*/ true, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.height.value_or(0.0f));
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.height = static_cast<float>(std::get<double>(v));
        },
    },

    // ── 间距（变化后触发 re-layout）──
    [static_cast<int>(PropId::padding)] = {
        PropId::padding, /*layoutAffecting*/ true, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return p.padding;
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.padding = std::get<EdgeInsets>(v);
        },
    },
    [static_cast<int>(PropId::margin)] = {
        PropId::margin, /*layoutAffecting*/ true, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return p.margin;
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.margin = std::get<EdgeInsets>(v);
        },
    },

    // ── 位置（绝对定位，不影响兄弟节点布局）──
    [static_cast<int>(PropId::x)] = {
        PropId::x, false, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.x);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.x = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::y)] = {
        PropId::y, false, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.y);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.y = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::absTop)] = {
        PropId::absTop, false, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.absTop);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.absTop = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::absLeft)] = {
        PropId::absLeft, false, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.absLeft);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.absLeft = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::absRight)] = {
        PropId::absRight, false, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.absRight);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.absRight = static_cast<float>(std::get<double>(v));
        },
    },
    [static_cast<int>(PropId::absBottom)] = {
        PropId::absBottom, false, false,
        /*reader*/ [](const ViewProps& p) -> TypedProp {
            return static_cast<double>(p.absBottom);
        },
        /*writer*/ [](ViewProps& p, const TypedProp& v) {
            p.absBottom = static_cast<float>(std::get<double>(v));
        },
    },

    // ── 文字 ──
    [static_cast<int>(PropId::textColor)] = {
        PropId::textColor, false, true,
        /*reader*/ [](const ViewProps&) -> TypedProp {
            // textColor 不属于 ViewProps，由子类覆盖
            return Color{0, 0, 0, 255};
        },
        /*writer*/ [](ViewProps&, const TypedProp&) {
            // 子类覆盖
        },
    },
    [static_cast<int>(PropId::fontSize)] = {
        PropId::fontSize, false, false,
        /*reader*/ [](const ViewProps&) -> TypedProp {
            return 0.0; // 子类覆盖
        },
        /*writer*/ [](ViewProps&, const TypedProp&) {
            // 子类覆盖
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
        static const PropMeta empty{PropId::COUNT, false, false, nullptr, nullptr};
        return empty;
    }
    return kPropMetas[idx];
}

// ═══════════════════════════════════════════════════════════════════════════
// 属性名 ↔ PropId 双向转换
// ═══════════════════════════════════════════════════════════════════════════

static constexpr struct {
    std::string_view name;
    PropId           id;
} kPropNameMap[] = {
    // 规范名（优先匹配）
    {"opacity",      PropId::opacity},
    {"scale",        PropId::scale},
    {"visible",      PropId::visible},
    {"background",   PropId::background},
    {"borderRadius", PropId::borderRadius},
    {"borderWidth",  PropId::borderWidth},
    {"borderColor",  PropId::borderColor},
    {"shadow",       PropId::shadow},
    {"transform",    PropId::transform},
    {"translateX",   PropId::translateX},
    {"translateY",   PropId::translateY},
    {"rotate",       PropId::rotate},
    {"width",        PropId::width},
    {"height",       PropId::height},
    {"padding",      PropId::padding},
    {"margin",       PropId::margin},
    {"x",            PropId::x},
    {"y",            PropId::y},
    {"absTop",       PropId::absTop},
    {"absLeft",      PropId::absLeft},
    {"absRight",     PropId::absRight},
    {"absBottom",    PropId::absBottom},
    {"textColor",    PropId::textColor},
    {"fontSize",     PropId::fontSize},
    // 别名（备选匹配）
    {"bg",           PropId::background},
    {"backgroundColor", PropId::background},
    {"radius",       PropId::borderRadius},
    {"bw",           PropId::borderWidth},
    {"bc",           PropId::borderColor},
    {"w",            PropId::width},
    {"h",            PropId::height},
    {"tx",           PropId::translateX},
    {"ty",           PropId::translateY},
    
};

PropId propIdFromName(std::string_view name) {
    for (auto& entry : kPropNameMap) {
        if (entry.name == name) return entry.id;
    }
    return PropId::COUNT;
}

const char* propName(PropId id) {
    for (auto& entry : kPropNameMap) {
        if (entry.id == id) return entry.name.data();
    }
    return "?";
}