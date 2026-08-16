module;

#include <cstddef>
#include "quickjs.h"

module kwik.bridge.props_parser;

import kwik.core.color_parser;
import kwik.core.types;
import kwik.core.props;
import kwik.engine.js_value;
import kwik.core.types;
import kwik.core.props;
import kwik.engine.js_value;
import kwik.core.theme; // ThemeData::resolveToken — "@primary" token 解析

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
            return EdgeInsets(value.getArrayElement(3).toFloat(),     // left   = arr[3]
                              value.getArrayElement(0).toFloat(),     // top    = arr[0]
                              value.getArrayElement(1).toFloat(),     // right  = arr[1]
                              value.getArrayElement(2).toFloat());    // bottom = arr[2]
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
// parseGradient — 渐变字符串解析
//   "linear 90 #ff6b6b #ffd93d"   → Linear，角度 + 两色
//   "radial #ff6b6b #ffd93d"      → Radial，两色
// ═══════════════════════════════════════════════════════════════════════════
Gradient parseGradient(const std::string &str) {
    Gradient g;
    if (str.empty()) return g;
    std::istringstream iss(str);
    std::vector<std::string> parts;
    std::string token;
    while (iss >> token) parts.push_back(token);
    if (parts.empty()) return g;

    if (parts[0] == "linear" && parts.size() >= 4) {
        g.type = GradientType::Linear;
        try {
            g.angleDeg = std::stof(parts[1]);
        } catch (...) { g.angleDeg = 180.0f; }
        g.color0 = parseColor(parts[2]);    // 复用 kwik.core.color_parser
        g.color1 = parseColor(parts[3]);
    } else if (parts[0] == "radial" && parts.size() >= 3) {
        g.type = GradientType::Radial;
        g.color0 = parseColor(parts[1]);
        g.color1 = parseColor(parts[2]);
    }
    return g;
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
        // width 支持百分比："50%" → widthPct(0.5)；纯数字 → width(px)
        // 必须先 isString 拦截百分比：ex.get 对 "50%" 返回 true 且 toFloat()=NaN
        // → NaN 写进 result.width 污染布局链（此前"先 get 后 else"写法是 bug）
        if (ex.has("width")) {
            auto v = ex.raw().getProperty("width");
            if (v.isString()) {
                std::string s = v.toString();
                if (s.size() > 1 && s.back() == '%')
                    result.widthPct = std::stof(s.substr(0, s.size() - 1)) / 100.0f;    // "50%" → 0.5
                else
                    result.width = std::stof(s);    // 非百分比字符串按 px 数字解析
            } else if (v.isNumber()) {
                result.width = v.toFloat();
            }
        }
    }
    {
        // height 百分比同上（isString 拦截，避免 NaN 污染）
        if (ex.has("height")) {
            auto v = ex.raw().getProperty("height");
            if (v.isString()) {
                std::string s = v.toString();
                if (s.size() > 1 && s.back() == '%')
                    result.heightPct = std::stof(s.substr(0, s.size() - 1)) / 100.0f;    // "50%" → 0.5
                else
                    result.height = std::stof(s);
            } else if (v.isNumber()) {
                result.height = v.toFloat();
            }
        }
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
    ex.get("transitionDuration", result.transitionDuration);

    {
        // scale 已并入 Transform：独立 scale 属性合并进 transform.scale（默认 1.0）
        float s = 1.0f;
        if (ex.get("scale", s) && s != 1.0f) {
            if (!result.transform) result.transform = Transform{};
            result.transform->scale = s;
        }
    }

    {
        float tmp = 0;
        if (ex.get("rotate", tmp) && std::isfinite(tmp)) {
            if (!result.transform) result.transform = Transform{};
            result.transform->rotate = tmp;
        }
        if (ex.get("translateY", tmp) && std::isfinite(tmp)) {
            if (!result.transform) result.transform = Transform{};
            result.transform->translateY = tmp;
        }
    }

    if (ex.has("transform")) {
        // transform 序列化为 "tx,ty,rot,scale"（逗号分隔，向后兼容 "tx,ty"）
        std::string s = ex.raw().getProperty("transform").toString();
        Transform t;
        size_t c1 = s.find(',');
        if (c1 == std::string::npos) {
            t.translateX = std::stof(s);
        } else {
            t.translateX = std::stof(s.substr(0, c1));
            size_t c2 = s.find(',', c1 + 1);
            if (c2 == std::string::npos) {
                t.translateY = std::stof(s.substr(c1 + 1));
            } else {
                t.translateY = std::stof(s.substr(c1 + 1, c2 - c1 - 1));
                size_t c3 = s.find(',', c2 + 1);
                if (c3 == std::string::npos) {
                    t.rotate = std::stof(s.substr(c2 + 1));
                } else {
                    t.rotate = std::stof(s.substr(c2 + 1, c3 - c2 - 1));
                    t.scale = std::stof(s.substr(c3 + 1));
                }
            }
        }
        result.transform = t;
    }

    if (ex.has("shadow")) result.shadow = parseShadow(ex.raw().getProperty("shadow").toString());
    if (ex.has("gradient")) result.gradient = parseGradient(ex.raw().getProperty("gradient").toString());
    ex.get("flexGrow", result.flexGrow);
    {
        float tmp = 0;
        if (ex.get("flex", tmp)) result.flexGrow = tmp;
    }
    ex.get("flexBasis", result.flexBasis);
    ex.get("flexShrink", result.flexShrink);
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
    ex.get("wordWrap", result.wordWrap);
    ex.get("maxLines", result.maxLines);
    ex.get("ellipsis", result.ellipsis);
    ex.get("lineHeight", result.lineHeight);
    ex.getEnum("verticalAlign", result.verticalAlign,
               {
                   {"center", TextVerticalAlign::Center},
                   {"bottom", TextVerticalAlign::Bottom},
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
    // ex.get("pressedScale", result.pressedScale);
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
    ex.getEnum("flexWrap", result.flexWrap,
               {
                   {"wrap", FlexWrap::Wrap},
               });
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
    ex.get("value", result.value);    // 触发 tryRecordBinding → 激活 propMeta 绑定标记
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
    if (v.isString()) { return parseColor(v.toString()); }
    if (v.isArray() && v.getArrayLength() >= 3) {
        uint8_t r = static_cast<uint8_t>(v.getArrayElement(0).toInt());
        uint8_t g = static_cast<uint8_t>(v.getArrayElement(1).toInt());
        uint8_t b = static_cast<uint8_t>(v.getArrayElement(2).toInt());
        uint8_t a = v.getArrayLength() >= 4 ? static_cast<uint8_t>(v.getArrayElement(3).toInt()) : 255;
        return {r, g, b, a};
    }
    return Color::transparent();
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
    ex.get("vertical", result.vertical);
    ex.get("thumbColor", result.thumbColor);
    ex.get("thumbBorderColor", result.thumbBorderColor);
    ex.get("showThumb", result.showThumb);
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

// ============================================================================
// parseTableProps — 解析表格属性
// ============================================================================
TableProps parseTableProps(PropsExtractor &ex) {
    TableProps tp;

    ex.get("headerColor", tp.headerColor);
    ex.get("headerTextColor", tp.headerTextColor);
    ex.get("stripeColor", tp.stripeColor);
    ex.get("rowTextColor", tp.rowTextColor);
    ex.get("borderColor", tp.borderColor);
    ex.get("sortArrowColor", tp.sortArrowColor);
    ex.get("headerHeight", tp.headerHeight);
    ex.get("rowHeight", tp.rowHeight);
    ex.get("fontSize", tp.fontSize);
    ex.get("borderWidth", tp.borderWidth);
    ex.get("showHeader", tp.showHeader);
    ex.get("striped", tp.striped);

    // 解析 columns 数组
    if (ex.has("columns")) {
        auto colsVal = ex.raw().getProperty("columns");
        if (colsVal.isArray()) {
            int len = colsVal.getArrayLength();
            for (int i = 0; i < len; ++i) {
                auto colVal = colsVal.getArrayElement(i);
                if (!colVal.isObject()) continue;

                ColumnDef col;
                PropsExtractor colEx(colVal);

                colEx.get("title", col.title);
                colEx.get("key", col.key);
                colEx.get("width", col.width);
                colEx.get("flex", col.flex);
                colEx.getEnum("align", col.align,
                              {
                                  {"left", std::string("left")},
                                  {"center", std::string("center")},
                                  {"right", std::string("right")},
                              });

                tp.columns.push_back(std::move(col));
            }
        }
    }

    return tp;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseTextViewProps
// ═══════════════════════════════════════════════════════════════════════════
TextViewProps parseTextViewProps(PropsExtractor &ex) {
    TextViewProps r;
    ex.get("value", r.value);
    ex.get("cursorColor", r.cursorColor);
    ex.get("selectionColor", r.selectionColor);
    ex.get("focusedBorderColor", r.focusedBorderColor);
    ex.get("maxLength", r.maxLength);
    ex.get("readOnly", r.readOnly);
    ex.get("placeholder", r.placeholder);
    ex.get("placeholderColor", r.placeholderColor);
    ex.get("placeholderFontSize", r.placeholderFontSize);

    // ── 解析 content 数组 ──
    if (ex.has("content")) {
        auto arr = ex.raw().getProperty("content");
        if (arr.isArray()) {
            int len = arr.getArrayLength();
            for (int i = 0; i < len; ++i) {
                auto item = arr.getArrayElement(i);
                if (!item.isObject()) continue;

                PropsExtractor iex(item);
                TextRun run;

                iex.get("text", run.text);
                iex.get("fontSize", run.style.fontSize);

                iex.getEnum("fontWeight", run.style.fontWeight,
                            {{"bold", FontWeight::Bold}, {"normal", FontWeight::Normal}});
                iex.getEnum("fontStyle", run.style.fontStyle,
                            {{"italic", FontStyle::Italic}, {"normal", FontStyle::Normal}});
                iex.get("textColor", run.style.textColor);
                iex.get("underline", run.style.underline);
                iex.get("strikethrough", run.style.strikethrough);

                r.content.push_back(std::move(run));
            }
        }
    }
    return r;
}

// TextViewProps parseTextViewProps(const JSValueRef &pv) — JSValueRef 兼容重载
TextViewProps parseTextViewProps(const JSValueRef &pv) {
    TypedPropMap meta;
    PropsExtractor ex(pv, &meta);
    return parseTextViewProps(ex);
}

// ════════════════════════════════════════════════════════
// parseTabsProps
// ════════════════════════════════════════════════════════
/**
 * @brief 解析 Tabs 专有属性
 * @param ex PropsExtractor 引用
 * @return 填充后的 TabsProps
 */
TabsProps parseTabsProps(PropsExtractor &ex) {
    TabsProps result;
    ex.get("selectedIndex", result.selectedIndex);
    ex.get("fontSize", result.fontSize);
    ex.get("indicatorHeight", result.indicatorHeight);
    ex.get("tabSpacing", result.tabSpacing);
    ex.get("tabColor", result.tabColor);
    ex.get("activeColor", result.activeColor);
    ex.get("tabBackground", result.tabBackground);
    ex.get("activeTabBackground", result.activeTabBackground);
    ex.get("indicatorColor", result.indicatorColor);

    // 解析 items 数组
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

// ============================================================================
// parseStackIndexProps — 解析 StackIndex 专有属性
// ============================================================================
StackIndexProps parseStackIndexProps(PropsExtractor &ex) {
    StackIndexProps sp;
    ex.get<int>("index", sp.index);
    return sp;
}

// ══════════════════════════════════════════════════════════════
// LayerProps 解析 — 统一浮层
// ══════════════════════════════════════════════════════════════
LayerProps parseLayerProps(PropsExtractor &ex) {
    LayerProps lp;
    ex.get("active", lp.active);
    ex.get("modal", lp.modal);
    ex.get("maskClosable", lp.maskClosable);
    ex.get("transparent", lp.transparent);
    ex.get("width", lp.width);
    ex.get("height", lp.height);
    ex.get("borderRadius", lp.borderRadius);
    ex.get("offsetX", lp.offsetX);
    ex.get("offsetY", lp.offsetY);
    ex.get("position", lp.position);
    ex.get("anchor", lp.anchor);
    // 颜色
    if (ex.has("maskColor")) {
        auto val = ex.raw().getProperty("maskColor");
        if (!val.isNull() && !val.isUndefined()) lp.maskColor = convertTo<Color>(val);
    }
    if (ex.has("background")) {
        auto val = ex.raw().getProperty("background");
        if (!val.isNull() && !val.isUndefined()) lp.background = convertTo<Color>(val);
    }
    // padding（支持数值 / [all] / [h,v] / [l,t,r,b] / 对象，与 ViewProps padding 一致）
    if (ex.has("padding")) { lp.padding = parseEdgeInsets(ex.raw().getProperty("padding")); }
    return lp;
}

ScrollViewProps parseScrollViewProps(PropsExtractor &ex) {
    ScrollViewProps result;
    // direction: "vertical"(默认) / "horizontal" / "both"
    ex.getEnum("direction", result.direction,
               {
                   {"vertical", ScrollDirection::Vertical},
                   {"horizontal", ScrollDirection::Horizontal},
                   {"both", ScrollDirection::Both},
               });
    ex.get("showScrollbar", result.showScrollbar);
    ex.get("scrollbarThickness", result.scrollbarThickness);
    ex.get("scrollbarColor", result.scrollbarColor);
    ex.get("scrollbarTrackColor", result.scrollbarTrackColor);
    ex.get("scrollStep", result.scrollStep);
    return result;
}

// ════════════════════════════════════════════════════════
// parseTreeNode — 递归解析单个树节点（JSValue 嵌套对象）
// ════════════════════════════════════════════════════════
namespace {

TreeNodeData parseTreeNode(const JSValueRef &v) {
    TreeNodeData n;
    n.key = v.getProperty("key").toString();
    n.title = v.getProperty("title").toString();
    n.icon = v.getProperty("icon").toString();
    n.checked = v.getProperty("checked").toBool();
    n.expanded = v.getProperty("expanded").toBool();
    auto children = v.getProperty("children");
    if (children.isArray()) {
        int len = children.getArrayLength();
        for (int i = 0; i < len; ++i) n.children.push_back(parseTreeNode(children.getArrayElement(i)));
    }
    return n;
}

}    // namespace

// ════════════════════════════════════════════════════════
// parseTreeMenuProps
// ════════════════════════════════════════════════════════
TreeMenuProps parseTreeMenuProps(PropsExtractor &ex) {
    TreeMenuProps result;
    if (ex.has("nodes")) {
        auto nodesVal = ex.raw().getProperty("nodes");
        if (nodesVal.isArray()) {
            int len = nodesVal.getArrayLength();
            for (int i = 0; i < len; ++i) result.nodes.push_back(parseTreeNode(nodesVal.getArrayElement(i)));
        }
    }
    ex.get("rowHeight", result.rowHeight);
    ex.get("indent", result.indent);
    ex.get("showCheckbox", result.showCheckbox);
    ex.get("showIcon", result.showIcon);
    ex.get("textColor", result.textColor);
    ex.get("iconColor", result.iconColor);
    ex.get("checkboxColor", result.checkboxColor);
    ex.get("hoverBackground", result.hoverBackground);
    ex.get("arrowColor", result.arrowColor);
    return result;
}

LazyListProps parseLazyListProps(PropsExtractor &ex) {
    LazyListProps result;
    // 固定模式：itemHeight(纵)/itemWidth(横) > 0 触发
    ex.get("itemHeight", result.itemHeight);
    ex.get("itemWidth", result.itemWidth);
    // 可变模式：estimatedItemSize 兜底
    ex.get("estimatedItemSize", result.estimatedItemSize);
    ex.get("overscan", result.overscan);
    // 分割线
    ex.get("dividerHeight", result.dividerHeight);
    ex.get("dividerColor", result.dividerColor);
    return result;
}

KeyboardProps parseKeyboardProps(PropsExtractor &ex) {
    KeyboardProps result;
    ex.get("visible", result.visible);
    // layout: "text" / "number" / "symbol"
    std::string layoutStr;
    ex.get("layout", layoutStr);
    if (layoutStr == "number")
        result.layout = KeyboardLayout::Number;
    else if (layoutStr == "symbol")
        result.layout = KeyboardLayout::Symbol;
    else
        result.layout = KeyboardLayout::Text;    // 默认 text
    ex.get("keyHeight", result.keyHeight);
    ex.get("background", result.background);
    ex.get("keyBackground", result.keyBackground);
    ex.get("keyActiveBackground", result.keyActiveBackground);
    ex.get("keyTextColor", result.keyTextColor);
    ex.get("keyFontSize", result.keyFontSize);
    ex.get("keyGap", result.keyGap);
    ex.get("keyRadius", result.keyRadius);
    ex.get("panelRadius", result.panelRadius);
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseDateTimePickerProps
// ═══════════════════════════════════════════════════════════════════════════
DateTimePickerProps parseDateTimePickerProps(PropsExtractor &ex) {
    DateTimePickerProps result;
    ex.get("placeholder", result.placeholder);
    ex.get("value", result.value);    // 触发 tryRecordBinding → 激活 propMeta 绑定标记
    ex.get("mode", result.mode);
    ex.get("fontSize", result.fontSize);
    ex.get("cellSize", result.cellSize);
    ex.get("wheelItemHeight", result.wheelItemHeight);
    ex.get("wheelColWidth", result.wheelColWidth);
    ex.get("wheelVisibleRows", result.wheelVisibleRows);
    ex.get("textColor", result.textColor);
    ex.get("placeholderColor", result.placeholderColor);
    ex.get("arrowColor", result.arrowColor);
    ex.get("panelBackground", result.panelBackground);
    ex.get("headerColor", result.headerColor);
    ex.get("weekdayColor", result.weekdayColor);
    ex.get("todayColor", result.todayColor);
    ex.get("selectedBackground", result.selectedBackground);
    ex.get("selectedTextColor", result.selectedTextColor);
    ex.get("hoverBackground", result.hoverBackground);
    ex.get("outOfMonthColor", result.outOfMonthColor);
    ex.get("navArrowColor", result.navArrowColor);
    ex.get("wheelDimColor", result.wheelDimColor);
    ex.get("separatorColor", result.separatorColor);
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseChartProps
// ═══════════════════════════════════════════════════════════════════════════
ChartProps parseChartProps(PropsExtractor &ex) {
    ChartProps result;
    ex.get("type", result.type);
    ex.get("value", result.value);
    ex.get("duration", result.duration);
    ex.get("strokeWidth", result.strokeWidth);
    ex.get("showGrid", result.showGrid);
    ex.get("showLabels", result.showLabels);
    ex.get("showLegend", result.showLegend);
    ex.get("gridColor", result.gridColor);
    ex.get("labelColor", result.labelColor);
    ex.get("legendColor", result.legendColor);
    ex.get("emptyColor", result.emptyColor);

    // ── categories — 字符串数组 ──
    if (ex.has("categories")) {
        auto arr = ex.raw().getProperty("categories");
        if (arr.isArray()) {
            for (int i = 0; i < arr.getArrayLength(); ++i)
                result.categories.push_back(arr.getArrayElement(i).toString());
        }
    }

    // ── series — 嵌套对象数组 [{label, data, color}] ──
    if (ex.has("series")) {
        auto arr = ex.raw().getProperty("series");
        if (arr.isArray()) {
            for (int i = 0; i < arr.getArrayLength(); ++i) {
                auto s = arr.getArrayElement(i);
                ChartSeries cs;
                auto lv = s.getProperty("label");
                if (!lv.isNull() && !lv.isUndefined()) cs.label = lv.toString();
                auto cv = s.getProperty("color");
                if (!cv.isNull() && !cv.isUndefined()) cs.color = convertTo<Color>(cv);
                auto dv = s.getProperty("data");
                if (dv.isArray()) {
                    for (int j = 0; j < dv.getArrayLength(); ++j) cs.data.push_back(dv.getArrayElement(j).toFloat());
                }
                result.series.push_back(std::move(cs));
            }
        }
    }

    // ── gauge — 仪表盘子对象 {min,max,start,sweep,ticks,segments:[{value,color}]} ──
    if (ex.has("gauge")) {
        auto gv = ex.raw().getProperty("gauge");
        if (gv.isObject()) {
            auto &g = result.gauge;
            auto getF = [&](const char *k, float &v) {
                auto p = gv.getProperty(k);
                if (!p.isNull() && !p.isUndefined()) v = p.toFloat();
            };
            getF("min", g.min);
            getF("max", g.max);
            getF("start", g.start);
            getF("sweep", g.sweep);
            getF("trackRatio", g.trackRatio);    // 外环带宽比例
            getF("innerRatio", g.innerRatio);    // 内环带宽比例
            // ── 指针模式（pointer/needleStyle/needleColor/hubColor/hubRadius/needleWidth）──
            auto ptrV = gv.getProperty("pointer");
            if (!ptrV.isNull() && !ptrV.isUndefined()) g.pointer = ptrV.toBool();
            auto nsV = gv.getProperty("needleStyle");
            if (nsV.isString()) g.needleStyle = nsV.toString();
            auto ncV = gv.getProperty("needleColor");
            if (!ncV.isNull() && !ncV.isUndefined()) g.needleColor = convertTo<Color>(ncV);
            auto hcV = gv.getProperty("hubColor");
            if (!hcV.isNull() && !hcV.isUndefined()) g.hubColor = convertTo<Color>(hcV);
            getF("hubRadius", g.hubRadius);
            getF("needleWidth", g.needleWidth);

            auto tv = gv.getProperty("ticks");
            if (!tv.isNull() && !tv.isUndefined()) g.ticks = tv.toInt();
            auto sv = gv.getProperty("segments");
            if (sv.isArray()) {
                for (int i = 0; i < sv.getArrayLength(); ++i) {
                    auto s = sv.getArrayElement(i);
                    GaugeSegment seg;
                    auto valV = s.getProperty("value");
                    if (!valV.isNull() && !valV.isUndefined()) seg.value = valV.toFloat();
                    auto colV = s.getProperty("color");
                    if (!colV.isNull() && !colV.isUndefined()) seg.color = convertTo<Color>(colV);
                    g.segments.push_back(std::move(seg));
                }
            }
        }
    }
    return result;
}

// ════════════════════════════════════════════════════════
// parseProgressRingProps
// ════════════════════════════════════════════════════════
ProgressRingProps parseProgressRingProps(PropsExtractor &ex) {
    ProgressRingProps result;
    ex.get("value", result.value);    // ref 绑定自动注册（binding_registry 增量链路）
    ex.get("min", result.min);
    ex.get("max", result.max);
    ex.get("trackColor", result.trackColor);
    ex.get("trackThickness", result.trackThickness);
    ex.get("thickness", result.thickness);
    ex.get("startColor", result.startColor);
    ex.get("endColor", result.endColor);
    ex.get("startAngle", result.startAngle);
    ex.get("sweep", result.sweep);
    ex.get("roundCap", result.roundCap);
    return result;
}