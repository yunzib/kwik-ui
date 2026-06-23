module;

#include <cstddef>
#include "quickjs.h"

module kwik.bridge.props_parser;

import kwik.bridge.color_parser;
import kwik.core.types;
import kwik.element.props;
import kwik.engine.js_value;
import kwik.bridge.color_parser;

// ═══════════════════════════════════════════════════════════════════════════
// parseEdgeInsets — 多重形态解析（数值/数组/对象）
// ═══════════════════════════════════════════════════════════════════════════

EdgeInsets parseEdgeInsets(const JSValueRef &value) {
    if (value.isNull() || value.isUndefined()) { return EdgeInsets{}; }
    if (value.isNumber()) { return EdgeInsets(value.toFloat()); }
    if (value.isArray()) {
        int len = value.getArrayLength();
        if (len == 1) return EdgeInsets(value.getArrayElement(0).toFloat());
        if (len == 2) {
            float h = value.getArrayElement(0).toFloat();
            float v = value.getArrayElement(1).toFloat();
            return EdgeInsets(h, v);
        }
        if (len >= 4) {
            return EdgeInsets(value.getArrayElement(1).toFloat(), value.getArrayElement(2).toFloat(),
                              value.getArrayElement(3).toFloat(), value.getArrayElement(0).toFloat());
        }
    }
    if (value.isObject()) {
        return EdgeInsets(value.getProperty("left").toFloat(), value.getProperty("top").toFloat(),
                          value.getProperty("right").toFloat(), value.getProperty("bottom").toFloat());
    }
    return EdgeInsets{};
}

// ═══════════════════════════════════════════════════════════════════════════
// parseShadow — "offsetX offsetY blurRadius color" 字符串解析
// ═══════════════════════════════════════════════════════════════════════════

Shadow parseShadow(const std::string &str) {
    if (str.empty()) return Shadow{};
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
        std::string blurStr = parts[2];
        if (blurStr.size() > 2 && blurStr.substr(blurStr.size() - 2) == "px")
            blurStr = blurStr.substr(0, blurStr.size() - 2);
        shadow.blurRadius = std::stof(blurStr);
    }
    if (parts.size() >= 4) {
        std::string colorStr;
        for (size_t i = 3; i < parts.size(); i++) colorStr += parts[i];
        shadow.color = parseColor(colorStr);
    }
    return shadow;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseBorderStyle
// ═══════════════════════════════════════════════════════════════════════════

BorderStyle parseBorderStyle(const std::string &str) {
    if (str == "solid") return BorderStyle::Solid;
    if (str == "dashed") return BorderStyle::Dashed;
    return BorderStyle::None;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseViewProps
// ═══════════════════════════════════════════════════════════════════════════

ViewProps parseViewProps(PropsExtractor &ex) {
    ViewProps result;
    ex.get("id", result.id);
    {
        float tmp = 0;
        if (ex.get("width", tmp)) result.width = tmp;
    }
    {
        float tmp = 0;
        if (ex.get("height", tmp)) result.height = tmp;
    }
    ex.get("background", result.background);
    ex.get("borderRadius", result.borderRadius);
    ex.get("borderWidth", result.borderWidth);
    ex.get("borderColor", result.borderColor);
    if (ex.has("borderStyle")) result.borderStyle = parseBorderStyle(ex.raw().getProperty("borderStyle").toString());
    if (ex.has("padding")) result.padding = parseEdgeInsets(ex.raw().getProperty("padding"));
    if (ex.has("margin")) result.margin = parseEdgeInsets(ex.raw().getProperty("margin"));
    ex.get("visible", result.visible);
    ex.get("opacity", result.opacity);
    if (ex.has("shadow")) result.shadow = parseShadow(ex.raw().getProperty("shadow").toString());
    ex.get("flexGrow", result.flexGrow);
    {
        float tmp = 0;
        if (ex.get("flex", tmp)) result.flexGrow = tmp;
    }
    ex.get("flexBasis", result.flexBasis);
    {
        int tmp = 0;
        if (ex.get("gridRow", tmp)) result.gridRow = tmp;
        if (ex.get("gridColumn", tmp)) result.gridColumn = tmp;
        if (ex.get("gridRowSpan", tmp)) result.gridRowSpan = std::max(1, tmp);
        if (ex.get("gridColumnSpan", tmp)) result.gridColumnSpan = std::max(1, tmp);
    }
    if (ex.has("position")) result.absolute = (ex.raw().getProperty("position").toString() == "absolute");
    ex.get("top", result.absTop);
    ex.get("left", result.absLeft);
    ex.get("right", result.absRight);
    ex.get("bottom", result.absBottom);
    ex.getEnum("align", result.align,
               {
                   {"topLeft", Align::TopLeft},
                   {"topCenter", Align::TopCenter},
                   {"topRight", Align::TopRight},
                   {"centerLeft", Align::CenterLeft},
                   {"center", Align::Center},
                   {"centerRight", Align::CenterRight},
                   {"bottomLeft", Align::BottomLeft},
                   {"bottomCenter", Align::BottomCenter},
                   {"bottomRight", Align::BottomRight},
               });
    {
        float tmp = 0;
        if (ex.get("x", tmp)) {
            result.x = tmp;
            result.hasExplicitX = true;
        }
        if (ex.get("y", tmp)) {
            result.y = tmp;
            result.hasExplicitY = true;
        }
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseTextContent
// ═══════════════════════════════════════════════════════════════════════════

TextContent parseTextContent(PropsExtractor &ex) {
    TextContent result;
    ex.get("text", result.text);
    ex.get("fontSize", result.fontSize);
    ex.get("fontFamily", result.fontFamily);
    ex.get("color", result.textColor);
    ex.getEnum("fontWeight", result.fontWeight,
               {
                   {"bold", FontWeight::Bold},
                   {"light", FontWeight::Light},
                   {"medium", FontWeight::Medium},
               });
    ex.getEnum("fontStyle", result.fontStyle,
               {
                   {"italic", FontStyle::Italic},
                   {"oblique", FontStyle::Oblique},
               });
    ex.getEnum("textAlign", result.textAlign,
               {
                   {"center", TextAlign::Center},
                   {"right", TextAlign::Right},
                   {"justify", TextAlign::Justify},
               });
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseButtonState
// ═══════════════════════════════════════════════════════════════════════════

ButtonStateProps parseButtonState(PropsExtractor &ex) {
    ButtonStateProps result;
    ex.get("hoverBackground", result.hoverBackground);
    ex.get("pressedBackground", result.pressedBackground);
    ex.get("pressedScale", result.pressedScale);
    ex.get("hoverBorderColor", result.hoverBorderColor);
    ex.get("pressedBorderColor", result.pressedBorderColor);
    if (ex.has("hoverShadow")) result.hoverShadow = parseShadow(ex.raw().getProperty("hoverShadow").toString());
    if (ex.has("pressedShadow")) result.pressedShadow = parseShadow(ex.raw().getProperty("pressedShadow").toString());
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseContainerProps
// ═══════════════════════════════════════════════════════════════════════════

ContainerProps parseContainerProps(PropsExtractor &ex) {
    ContainerProps result;
    if (ex.has("direction"))
        result.flexDirection =
            (ex.raw().getProperty("direction").toString() == "column") ? FlexDirection::Column : FlexDirection::Row;
    ex.getEnum("justifyContent", result.mainAxisAlignment,
               {
                   {"center", LayoutAlign::Center},
                   {"end", LayoutAlign::End},
                   {"spaceBetween", LayoutAlign::SpaceBetween},
                   {"spaceAround", LayoutAlign::SpaceAround},
                   {"spaceEvenly", LayoutAlign::SpaceEvenly},
               });
    ex.getEnum("alignItems", result.crossAxisAlignment,
               {
                   {"center", CrossAlign::Center},
                   {"end", CrossAlign::End},
                   {"stretch", CrossAlign::Stretch},
               });
    ex.get("gap", result.gap);
    {
        int tmp = 0;
        if (ex.get("columns", tmp)) result.gridCols = std::max(1, tmp);
        if (ex.get("rows", tmp)) result.gridRows = std::max(1, tmp);
    }
    ex.get("columnGap", result.columnGap);
    ex.get("rowGap", result.rowGap);
    ex.getEnum("scrollDirection", result.scrollDir,
               {
                   {"horizontal", ScrollDirection::Horizontal},
                   {"both", ScrollDirection::Both},
               });
    ex.get("dividerColor", result.dividerColor);
    ex.get("dividerHeight", result.dividerHeight);
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseImageProps
// ═══════════════════════════════════════════════════════════════════════════

ImageProps parseImageProps(PropsExtractor &ex) {
    ImageProps result;
    if (ex.has("src")) {
        result.src = ex.raw().getProperty("src").toString();
        result.source = ImageSource::File;
    }
    ex.getEnum("fit", result.fit,
               {
                   {"fill", ImageFit::Fill},
                   {"contain", ImageFit::Contain},
                   {"none", ImageFit::None},
               });
    ex.get("opacity", result.imageOpacity);
    if (ex.has("data")) {
        auto dataVal = ex.raw().getProperty("data");
        JSContext *ctx = dataVal.context();
        JSValue raw = dataVal.raw();
        if (!JS_IsNull(raw) && !JS_IsUndefined(raw)) {
            size_t byteLen = 0;
            uint8_t *buf = JS_GetArrayBuffer(ctx, &byteLen, raw);
            if (buf && byteLen > 0) {
                result.data.assign(buf, buf + byteLen);
                result.source = ImageSource::Buffer;
            }
        }
    }
    {
        int tmp = 0;
        if (ex.get("width", tmp)) result.bufferWidth = tmp;
        if (ex.get("height", tmp)) result.bufferHeight = tmp;
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseInputProps
// ═══════════════════════════════════════════════════════════════════════════

InputProps parseInputProps(PropsExtractor &ex) {
    InputProps ip;
    ex.get("value", ip.value);
    ex.get("placeholder", ip.placeholder);
    ex.get("fontSize", ip.fontSize);
    ex.get("textColor", ip.textColor);
    ex.get("placeholderColor", ip.placeholderColor);
    ex.get("cursorColor", ip.cursorColor);
    ex.get("focusedBorderColor", ip.focusedBorderColor);
    ex.get("maxLength", ip.maxLength);
    ex.get("readOnly", ip.readOnly);
    if (ex.has("type")) ip.isPassword = (ex.raw().getProperty("type").toString() == "password");
    return ip;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseRadioButtonProps
// ═══════════════════════════════════════════════════════════════════════════

RadioButtonProps parseRadioButtonProps(PropsExtractor &ex) {
    RadioButtonProps result;
    ex.get("checked", result.checked);
    ex.get("group", result.group);
    ex.get("checkedColor", result.checkedColor);
    ex.get("uncheckedColor", result.uncheckedColor);
    ex.get("dotColor", result.dotColor);
    ex.get("radioSize", result.radioSize);
    ex.get("dotSize", result.dotSize);
    ex.get("ringWidth", result.ringWidth);
    ex.get("value", result.value);
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseRadioGroupProps
// ═══════════════════════════════════════════════════════════════════════════

RadioGroupProps parseRadioGroupProps(PropsExtractor &ex) {
    RadioGroupProps result;
    ex.get("name", result.name);
    ex.get("selected", result.selected);
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseCheckboxProps
// ═══════════════════════════════════════════════════════════════════════════

CheckboxProps parseCheckboxProps(PropsExtractor &ex) {
    CheckboxProps result;
    ex.get("checked", result.checked);
    ex.get("checkedColor", result.checkedColor);
    ex.get("uncheckedColor", result.uncheckedColor);
    ex.get("checkedFillColor", result.checkedFillColor);
    ex.get("checkMarkColor", result.checkMarkColor);
    ex.get("boxSize", result.boxSize);
    ex.get("borderRadius", result.borderRadius);
    ex.get("ringWidth", result.ringWidth);
    ex.get("textSpacing", result.textSpacing);
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseTextAreaProps
// ═══════════════════════════════════════════════════════════════════════════

TextAreaProps parseTextAreaProps(PropsExtractor &ex) {
    TextAreaProps result;
    ex.get("value", result.value);
    ex.get("placeholder", result.placeholder);
    ex.get("fontSize", result.fontSize);
    ex.get("rows", result.rows);
    ex.get("textColor", result.textColor);
    ex.get("placeholderColor", result.placeholderColor);
    ex.get("cursorColor", result.cursorColor);
    ex.get("focusedBorderColor", result.focusedBorderColor);
    ex.get("maxLength", result.maxLength);
    ex.get("readOnly", result.readOnly);
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseDropdownProps
// ═══════════════════════════════════════════════════════════════════════════

DropdownProps parseDropdownProps(PropsExtractor &ex) {
    DropdownProps result;
    ex.get("placeholder", result.placeholder);
    ex.get("selectedIndex", result.selectedIndex);
    ex.get("fontSize", result.fontSize);
    ex.get("itemHeight", result.itemHeight);
    ex.get("maxVisibleItems", result.maxVisibleItems);
    ex.get("textColor", result.textColor);
    ex.get("placeholderColor", result.placeholderColor);
    ex.get("arrowColor", result.arrowColor);
    ex.get("menuBackground", result.menuBackground);
    ex.get("hoverBackground", result.hoverBackground);
    ex.get("selectedBackground", result.selectedBackground);
    if (ex.has("items")) {
        auto itemsVal = ex.raw().getProperty("items");
        if (itemsVal.isArray()) {
            int len = itemsVal.getArrayLength();
            for (int i = 0; i < len; ++i) {
                auto item = itemsVal.getArrayElement(i);
                if (item.isString()) result.items.push_back(item.toString());
            }
        }
    }
    return result;
}

template <>
Color convertTo<Color>(const JSValueRef &v) {
    return parseColor(v.toString());
}

// ════════════════════════════════════════════════════════
// parseSliderProps
// ════════════════════════════════════════════════════════
SliderProps parseSliderProps(PropsExtractor &ex) {
    SliderProps result;
    ex.get("value", result.value);
    ex.get("min", result.min);
    ex.get("max", result.max);
    ex.get("step", result.step);
    ex.get("color", result.color);
    ex.get("trackColor", result.trackColor);
    ex.get("thumbSize", result.thumbSize);
    ex.get("trackHeight", result.trackHeight);
    return result;
}

// ════════════════════════════════════════════════════════
// parseProgressBarProps
// ════════════════════════════════════════════════════════
ProgressBarProps parseProgressBarProps(PropsExtractor &ex) {
    ProgressBarProps result;
    ex.get("value", result.value);
    ex.get("min", result.min);
    ex.get("max", result.max);
    ex.get("color", result.color);
    ex.get("trackColor", result.trackColor);
    ex.get("trackHeight", result.trackHeight);
    return result;
}

// ════════════════════════════════════════════════════════
// parseSwitchProps
// ════════════════════════════════════════════════════════
SwitchProps parseSwitchProps(PropsExtractor &ex) {
    SwitchProps result;
    ex.get("checked", result.checked);
    ex.get("checkedColor", result.checkedColor);
    ex.get("uncheckedColor", result.uncheckedColor);
    ex.get("thumbColor", result.thumbColor);
    ex.get("trackHeight", result.trackHeight);
    ex.get("thumbSize", result.thumbSize);
    return result;
}

LineProps parseLineProps(PropsExtractor &ex) {
    LineProps result;
    if (ex.has("direction")) {
        std::string dir = ex.raw().getProperty("direction").toString();
        if (dir == "vertical") result.direction = "vertical";
    }
    ex.get("strokeWidth", result.strokeWidth);
    ex.get("color", result.color);
    return result;
}

SpinnerProps parseSpinnerProps(PropsExtractor &ex) {
    SpinnerProps result;
    ex.get("color", result.color);
    ex.get("trackColor", result.trackColor);
    ex.get("size", result.size);
    ex.get("strokeWidth", result.strokeWidth);
    ex.get("arcLength", result.arcLength);
    return result;
}