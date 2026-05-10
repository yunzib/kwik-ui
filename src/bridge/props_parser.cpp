module;

#include <cstddef>

module kwik.bridge.props_parser;

import kwik.bridge.color_parser;
import kwik.core.types;
import kwik.element.props;
import kwik.engine.js_value;

EdgeInsets parseEdgeInsets(const JSValueRef &value) {
    if (value.isNull() || value.isUndefined()) { return EdgeInsets{}; }

    // 数值：四边相同
    if (value.isNumber()) { return EdgeInsets(value.toFloat()); }

    // 数组
    if (value.isArray()) {
        int len = value.getArrayLength();
        if (len == 1) {
            return EdgeInsets(value.getArrayElement(0).toFloat());
        } else if (len == 2) {
            float h = value.getArrayElement(0).toFloat();
            float v = value.getArrayElement(1).toFloat();
            return EdgeInsets(h, v);
        } else if (len >= 4) {
            return EdgeInsets(value.getArrayElement(0).toFloat(), value.getArrayElement(1).toFloat(),
                              value.getArrayElement(2).toFloat(), value.getArrayElement(3).toFloat());
        }
    }

    // 对象
    if (value.isObject()) {
        return EdgeInsets(value.getProperty("left").toFloat(), value.getProperty("top").toFloat(),
                          value.getProperty("right").toFloat(), value.getProperty("bottom").toFloat());
    }

    return EdgeInsets{};
}
Shadow parseShadow(const std::string &str) {
    Shadow shadow;

    std::istringstream iss(str);
    std::string token;
    std::vector<std::string> parts;

    while (iss >> token) { parts.push_back(token); }

    if (parts.size() >= 2) {
        shadow.offsetX = std::stof(parts[0]);
        shadow.offsetY = std::stof(parts[1]);
    }

    if (parts.size() >= 3) {
        // 移除 'px' 后缀
        std::string blurStr = parts[2];
        if (blurStr.size() > 2 && blurStr.substr(blurStr.size() - 2) == "px") {
            blurStr = blurStr.substr(0, blurStr.size() - 2);
        }
        shadow.blurRadius = std::stof(blurStr);
    }

    if (parts.size() >= 4) {
        // 剩余部分组成颜色字符串
        std::string colorStr;
        for (size_t i = 3; i < parts.size(); i++) { colorStr += parts[i]; }
        shadow.color = parseColor(colorStr);
    }

    return shadow;
}
BorderStyle parseBorderStyle(const std::string &str) {
    if (str == "solid") return BorderStyle::Solid;
    if (str == "dashed") return BorderStyle::Dashed;
    return BorderStyle::None;
}

ViewProps parseViewProps(const JSValueRef &props) {
    ViewProps result;
    if (!props.isObject()) { return result; }
    // 尺寸
    if (props.hasProperty("width")) {
        auto w = props.getProperty("width");
        if (!w.isNull() && !w.isUndefined()) { result.width = w.toFloat(); }
    }
    if (props.hasProperty("height")) {
        auto h = props.getProperty("height");
        if (!h.isNull() && !h.isUndefined()) { result.height = h.toFloat(); }
    }
    // 背景
    if (props.hasProperty("background")) { result.background = parseColor(props.getProperty("background").toString()); }
    // 圆角
    if (props.hasProperty("borderRadius")) { result.borderRadius = props.getProperty("borderRadius").toFloat(); }
    // 边框
    if (props.hasProperty("borderWidth")) { result.borderWidth = props.getProperty("borderWidth").toFloat(); }
    if (props.hasProperty("borderColor")) {
        result.borderColor = parseColor(props.getProperty("borderColor").toString());
    }
    if (props.hasProperty("borderStyle")) {
        result.borderStyle = parseBorderStyle(props.getProperty("borderStyle").toString());
    }
    // 间距
    if (props.hasProperty("padding")) { result.padding = parseEdgeInsets(props.getProperty("padding")); }
    if (props.hasProperty("margin")) { result.margin = parseEdgeInsets(props.getProperty("margin")); }
    // 可见性
    if (props.hasProperty("visible")) { result.visible = props.getProperty("visible").toBool(); }
    // 透明度
    if (props.hasProperty("opacity")) { result.opacity = props.getProperty("opacity").toFloat(); }
    // 阴影
    if (props.hasProperty("shadow")) { result.shadow = parseShadow(props.getProperty("shadow").toString()); }
    // ── Flex 子项 ──
    if (props.hasProperty("flexGrow")) result.flexGrow = props.getProperty("flexGrow").toFloat();
    if (props.hasProperty("flex")) result.flexGrow = props.getProperty("flex").toFloat();
    if (props.hasProperty("flexBasis")) result.flexBasis = props.getProperty("flexBasis").toFloat();
    // ── Grid 子项 ──
    if (props.hasProperty("gridRow")) result.gridRow = (int)props.getProperty("gridRow").toFloat();
    if (props.hasProperty("gridColumn")) result.gridColumn = (int)props.getProperty("gridColumn").toFloat();
    if (props.hasProperty("gridRowSpan"))
        result.gridRowSpan = std::max(1, (int)props.getProperty("gridRowSpan").toFloat());
    if (props.hasProperty("gridColumnSpan"))
        result.gridColumnSpan = std::max(1, (int)props.getProperty("gridColumnSpan").toFloat());
    // ── Stack 子项 ──
    if (props.hasProperty("position")) { result.absolute = (props.getProperty("position").toString() == "absolute"); }
    if (props.hasProperty("top")) result.absTop = props.getProperty("top").toFloat();
    if (props.hasProperty("left")) result.absLeft = props.getProperty("left").toFloat();
    if (props.hasProperty("right")) result.absRight = props.getProperty("right").toFloat();
    if (props.hasProperty("bottom")) result.absBottom = props.getProperty("bottom").toFloat();
    // ── 通用对齐 ──
    if (props.hasProperty("align")) {
        auto a = props.getProperty("align").toString();
        if (a == "topLeft")
            result.align = Align::TopLeft;
        else if (a == "topCenter")
            result.align = Align::TopCenter;
        else if (a == "topRight")
            result.align = Align::TopRight;
        else if (a == "centerLeft")
            result.align = Align::CenterLeft;
        else if (a == "center")
            result.align = Align::Center;
        else if (a == "centerRight")
            result.align = Align::CenterRight;
        else if (a == "bottomLeft")
            result.align = Align::BottomLeft;
        else if (a == "bottomCenter")
            result.align = Align::BottomCenter;
        else if (a == "bottomRight")
            result.align = Align::BottomRight;
    }
    if (props.hasProperty("x")) {
        result.x = props.getProperty("x").toFloat();
        result.hasExplicitX = true;
    }
    if (props.hasProperty("y")) {
        result.y = props.getProperty("y").toFloat();
        result.hasExplicitY = true;
    }
    return result;
}

TextContent parseTextContent(const JSValueRef &props) {
    TextContent result;
    if (!props.isObject()) { return result; }
    if (props.hasProperty("text")) { result.text = props.getProperty("text").toString(); }
    if (props.hasProperty("fontSize")) { result.fontSize = props.getProperty("fontSize").toFloat(); }
    if (props.hasProperty("fontFamily")) { result.fontFamily = props.getProperty("fontFamily").toString(); }
    if (props.hasProperty("color")) { result.textColor = parseColor(props.getProperty("color").toString()); }
    if (props.hasProperty("fontWeight")) {
        auto fw = props.getProperty("fontWeight").toString();
        if (fw == "bold")
            result.fontWeight = FontWeight::Bold;
        else if (fw == "light")
            result.fontWeight = FontWeight::Light;
        else if (fw == "medium")
            result.fontWeight = FontWeight::Medium;
    }
    if (props.hasProperty("fontStyle")) {
        auto fs = props.getProperty("fontStyle").toString();
        if (fs == "italic")
            result.fontStyle = FontStyle::Italic;
        else if (fs == "oblique")
            result.fontStyle = FontStyle::Oblique;
    }
    if (props.hasProperty("textAlign")) {
        auto ta = props.getProperty("textAlign").toString();
        if (ta == "center")
            result.textAlign = TextAlign::Center;
        else if (ta == "right")
            result.textAlign = TextAlign::Right;
        else if (ta == "justify")
            result.textAlign = TextAlign::Justify;
    }
    return result;
}
ButtonStateProps parseButtonState(const JSValueRef &props) {
    ButtonStateProps result;
    if (!props.isObject()) { return result; }
    if (props.hasProperty("hoverBackground")) {
        result.hoverBackground = parseColor(props.getProperty("hoverBackground").toString());
    }
    if (props.hasProperty("pressedBackground")) {
        result.pressedBackground = parseColor(props.getProperty("pressedBackground").toString());
    }
    if (props.hasProperty("pressedScale")) { result.pressedScale = props.getProperty("pressedScale").toFloat(); }
    if (props.hasProperty("hoverBorderColor")) {
        result.hoverBorderColor = parseColor(props.getProperty("hoverBorderColor").toString());
    }
    if (props.hasProperty("pressedBorderColor")) {
        result.pressedBorderColor = parseColor(props.getProperty("pressedBorderColor").toString());
    }
    if (props.hasProperty("hoverShadow")) {
        result.hoverShadow = parseShadow(props.getProperty("hoverShadow").toString());
    }
    if (props.hasProperty("pressedShadow")) {
        result.pressedShadow = parseShadow(props.getProperty("pressedShadow").toString());
    }
    return result;
}

ContainerProps parseContainerProps(const JSValueRef &props) {
    ContainerProps result;
    if (!props.isObject()) { return result; }
    if (props.hasProperty("direction")) {
        result.flexDirection =
            (props.getProperty("direction").toString() == "column") ? FlexDirection::Column : FlexDirection::Row;
    }
    if (props.hasProperty("justifyContent")) {
        auto a = props.getProperty("justifyContent").toString();
        if (a == "center")
            result.mainAxisAlignment = LayoutAlign::Center;
        else if (a == "end")
            result.mainAxisAlignment = LayoutAlign::End;
        else if (a == "spaceBetween")
            result.mainAxisAlignment = LayoutAlign::SpaceBetween;
        else if (a == "spaceAround")
            result.mainAxisAlignment = LayoutAlign::SpaceAround;
        else if (a == "spaceEvenly")
            result.mainAxisAlignment = LayoutAlign::SpaceEvenly;
    }
    if (props.hasProperty("alignItems")) {
        auto a = props.getProperty("alignItems").toString();
        if (a == "center")
            result.crossAxisAlignment = CrossAlign::Center;
        else if (a == "end")
            result.crossAxisAlignment = CrossAlign::End;
        else if (a == "stretch")
            result.crossAxisAlignment = CrossAlign::Stretch;
    }
    if (props.hasProperty("gap")) result.gap = props.getProperty("gap").toFloat();
    if (props.hasProperty("columns")) result.gridCols = std::max(1, (int)props.getProperty("columns").toFloat());
    if (props.hasProperty("rows")) result.gridRows = std::max(1, (int)props.getProperty("rows").toFloat());
    if (props.hasProperty("columnGap")) result.columnGap = props.getProperty("columnGap").toFloat();
    if (props.hasProperty("rowGap")) result.rowGap = props.getProperty("rowGap").toFloat();
    if (props.hasProperty("scrollDirection")) {
        auto d = props.getProperty("scrollDirection").toString();
        if (d == "horizontal")
            result.scrollDir = ScrollDirection::Horizontal;
        else if (d == "both")
            result.scrollDir = ScrollDirection::Both;
    }
    return result;
}