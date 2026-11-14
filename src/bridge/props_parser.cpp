module;

#include "quickjs.h"

module kwik.bridge.props_parser;

import kwik.core.color_parser;
import kwik.core.types;
import kwik.core.props;
import kwik.engine.js_value;
import kwik.core.theme; // ThemeData::resolveToken — "@primary" token 解析
import kwik.bridge.props_reflect;    // parseStructProps（C++26 反射通用解析）

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
                if (s.size() > 1 && s.back() == '%') {
                    float pct = std::stof(s.substr(0, s.size() - 1)) / 100.0f;    // "50%" → 0.5
                    if (std::isfinite(pct)) result.widthPct = pct;    // "nan%" 串拒绝
                } else {
                    float px = std::stof(s);    // 非百分比字符串按 px 数字解析
                    if (std::isfinite(px)) result.width = px;    // "nan"/"inf" 串拒绝
                }
            } else if (v.isNumber()) {
                float f = v.toFloat();
                if (std::isfinite(f)) result.width = f;    // JS 0/0 等 NaN 源拒绝
            }
        }
    }
    {
        // height 同 width：百分比/字符串/数值三路均做有限性拒绝
        if (ex.has("height")) {
            auto v = ex.raw().getProperty("height");
            if (v.isString()) {
                std::string s = v.toString();
                if (s.size() > 1 && s.back() == '%') {
                    float pct = std::stof(s.substr(0, s.size() - 1)) / 100.0f;
                    if (std::isfinite(pct)) result.heightPct = pct;
                } else {
                    float px = std::stof(s);
                    if (std::isfinite(px)) result.height = px;
                }
            } else if (v.isNumber()) {
                float f = v.toFloat();
                if (std::isfinite(f)) result.height = f;
            }
        }
    }
    ex.get("background", result.background);
    ex.get("backdropBlur", result.backdropBlur);
    ex.get("backdropRefraction", result.backdropRefraction);
    ex.get("backdropSpecular", result.backdropSpecular);
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
        std::size_t c1 = s.find(',');
        if (c1 == std::string::npos) {
            t.translateX = std::stof(s);
        } else {
            t.translateX = std::stof(s.substr(0, c1));
            std::size_t c2 = s.find(',', c1 + 1);
            if (c2 == std::string::npos) {
                t.translateY = std::stof(s.substr(c1 + 1));
            } else {
                t.translateY = std::stof(s.substr(c1 + 1, c2 - c1 - 1));
                std::size_t c3 = s.find(',', c2 + 1);
                if (c3 == std::string::npos) {
                    t.rotate = std::stof(s.substr(c2 + 1));
                } else {
                    t.rotate = std::stof(s.substr(c2 + 1, c3 - c2 - 1));
                    t.scale = std::stof(s.substr(c3 + 1));
                }
            }
        }
        // NaN/±Inf 拒绝（"nan" 串可被 stof 正常解析）：transform 会进
        // AABB/命中数学，非有限值沿图形链传播
        if (std::isfinite(t.translateX) && std::isfinite(t.translateY) && std::isfinite(t.rotate) && std::isfinite(t.scale))
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
    if (ex.has("align")) result.align = parseAlign(ex.raw().getProperty("align").toString());

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
    ex.get("selectable", result.selectable);
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseButtonState
// ═══════════════════════════════════════════════════════════════════════════

ButtonStateProps parseButtonState(PropsExtractor &ex) {
    ButtonStateProps result;
    result.hasBackground = ex.has("background");    // 显式 background（含透明）= 用户自主选择，主题兜底让位
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
                   {"cover", ImageFit::Cover},    // 20 号：此前未知串静默落枚举默认值，显式 "cover" 写不进
                   {"none", ImageFit::None},
               });
    ex.get("opacity", result.imageOpacity);
    if (ex.has("data")) {
        auto dataVal = ex.raw().getProperty("data");
        JSContext *ctx = dataVal.context();
        JSValue raw = dataVal.raw();
        if (!JS_IsNull(raw) && !JS_IsUndefined(raw)) {
            std::size_t byteLen = 0;
            std::uint8_t *buf = JS_GetArrayBuffer(ctx, &byteLen, raw);
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
    if (ex.has("type")) {
        // type 决定输入模式: "password" → 密码遮罩, "number" → 数字过滤
        // 两者同时设置时 password 优先 (遮蔽优先, 见 Input::onEvent 顺序)
        std::string t = ex.raw().getProperty("type").toString();
        ip.isPassword = (t == "password");
        ip.isNumber   = (t == "number");
    }
    // 数字模式约束 (optional, 未写则不生效)
    if (ex.has("min"))  ip.min  = ex.raw().getProperty("min").toFloat();
    if (ex.has("max"))  ip.max  = ex.raw().getProperty("max").toFloat();
    if (ex.has("step")) ip.step = ex.raw().getProperty("step").toFloat();
    return ip;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseRadioButtonProps
// ═══════════════════════════════════════════════════════════════════════════

RadioButtonProps parseRadioButtonProps(PropsExtractor &ex) {
    // 反射桥通用路径（成员名≡JS 属性名，零语义例外）；本批机械函数同此
    RadioButtonProps result;
    parseStructProps(ex, result);
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseRadioGroupProps
// ═══════════════════════════════════════════════════════════════════════════

RadioGroupProps parseRadioGroupProps(PropsExtractor &ex) {
    RadioGroupProps result;
    parseStructProps(ex, result);
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseCheckboxProps
// ═══════════════════════════════════════════════════════════════════════════

CheckboxProps parseCheckboxProps(PropsExtractor &ex) {
    CheckboxProps result;
    parseStructProps(ex, result);
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseTextAreaProps
// ═══════════════════════════════════════════════════════════════════════════

TextAreaProps parseTextAreaProps(PropsExtractor &ex) {
    TextAreaProps result;
    parseStructProps(ex, result);
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
        std::uint8_t r = static_cast<std::uint8_t>(v.getArrayElement(0).toInt());
        std::uint8_t g = static_cast<std::uint8_t>(v.getArrayElement(1).toInt());
        std::uint8_t b = static_cast<std::uint8_t>(v.getArrayElement(2).toInt());
        std::uint8_t a = v.getArrayLength() >= 4 ? static_cast<std::uint8_t>(v.getArrayElement(3).toInt()) : 255;
        return {r, g, b, a};
    }
    return Color::transparent();
}

// ════════════════════════════════════════════════════════
// parseSliderProps
// ════════════════════════════════════════════════════════
SliderProps parseSliderProps(PropsExtractor &ex) {
    // 反射桥首批试点：SliderProps 全成员为纯数据（float/bool/Color），零语义
    // 例外（PropsTraits<SliderProps> 缺省空 overrides）——逐成员通用路径。
    // 行为等价性由 js_tests::test_slider_reflect_parse 锁定
    SliderProps result;
    parseStructProps(ex, result);
    return result;
}

// ════════════════════════════════════════════════════════
// parseProgressBarProps
// ════════════════════════════════════════════════════════
ProgressBarProps parseProgressBarProps(PropsExtractor &ex) {
    ProgressBarProps result;
    parseStructProps(ex, result);
    return result;
}

// ════════════════════════════════════════════════════════
// parseSwitchProps
// ════════════════════════════════════════════════════════
SwitchProps parseSwitchProps(PropsExtractor &ex) {
    SwitchProps result;
    parseStructProps(ex, result);
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
    parseStructProps(ex, result);
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
    if (tp.rowHeight <= 0) tp.rowHeight = 32.0f;    // 显式 0/负值回退默认：行绘制步进与命中换算均以此做乘除，源头消除除零
    ex.get("fontSize", tp.fontSize);
    ex.get("borderWidth", tp.borderWidth);
    ex.get("showHeader", tp.showHeader);
    ex.get("striped", tp.striped);
    ex.get("showScrollbar", tp.showScrollbar);

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
    ex.get("linkColor", r.linkColor);
    ex.get("highlightColor", r.highlightColor);
    ex.get("lineHeight", r.lineHeight);
    if (ex.has("textAlign")) {
        std::string ta;
        ex.get("textAlign", ta);
        if (ta == "center" || ta == "middle")
            r.textAlign = 1;
        else if (ta == "right" || ta == "end")
            r.textAlign = 2;
        else
            r.textAlign = 0;
    }

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
                iex.get("link", run.link);

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
    parseStructProps(ex, sp);
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
    // key/icon 可缺省（title 实践必写）：undefined 经 toString() 成字面量
    // "undefined" 串——绘制侧 !icon.empty() 门拦不住，缺省节点每行画出
    // "undefined" 字样（docsite treemenu 页未写 icon 的节点全中）。缺省归空串
    auto strOrEmpty = [&v](const char *name) {
        auto p = v.getProperty(name);
        return (p.isUndefined() || p.isNull()) ? std::string{} : p.toString();
    };
    n.key = strOrEmpty("key");
    n.title = strOrEmpty("title");
    n.icon = strOrEmpty("icon");
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
    // ── 受控键：键存在即受控（Expansion open 同族口径）；值为展开 map /
    // 勾选 CSV 字符串。缺省键 = 内部自持，存量用法零影响
    if (ex.has("expanded")) {
        result.expandedControlled = true;
        ex.get("expanded", result.expanded);
    }
    if (ex.has("checked")) {
        result.checkedControlled = true;
        ex.get("checked", result.checked);
    }
    return result;
}

LazyListProps parseLazyListProps(PropsExtractor &ex) {
    LazyListProps result;
    // 固定/可变模式语义在消费端（LazyList 按值分派），解析层纯机械
    parseStructProps(ex, result);
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
    if (ex.has("panelPadding")) result.panelPadding = parseEdgeInsets(ex.raw().getProperty("panelPadding"));
    // 位置三件套（键位锚定批）：dock 模式 + 比例/偏移/坐标；参数规范化在属性行
    ex.get("offsetBottom", result.offsetBottom);
    ex.get("widthRatio", result.widthRatio);
    ex.get("x", result.x);
    ex.get("y", result.y);
    result.offsetBottom = std::max(result.offsetBottom, 0.0f);          // 上抬非负
    result.widthRatio = std::clamp(result.widthRatio, 0.0f, 1.0f);      // 比宽 [0,1]
    std::string dockStr;
    ex.get("dock", dockStr);
    if (dockStr == "floating")
        result.dock = KeyboardDock::Floating;
    else
        result.dock = KeyboardDock::Bottom;    // 默认 bottom（未知串同口径回退）
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseDateTimePickerProps
// ═══════════════════════════════════════════════════════════════════════════
DateTimePickerProps parseDateTimePickerProps(PropsExtractor &ex) {
    DateTimePickerProps result;
    parseStructProps(ex, result);    // value 的 tryRecordBinding 经 get<T> 自动继承
    return result;
}

// Chart 属性解析已随组件迁移 extensions/chart/impl/bindings.cpp（扩展自有解析）

// ════════════════════════════════════════════════════════
// parseProgressRingProps
// ════════════════════════════════════════════════════════
ProgressRingProps parseProgressRingProps(PropsExtractor &ex) {
    ProgressRingProps result;
    parseStructProps(ex, result);    // value 的 ref 绑定自动注册经 get<T> 继承
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseSpinBoxProps
// ═══════════════════════════════════════════════════════════════════════════
SpinBoxProps parseSpinBoxProps(PropsExtractor &ex) {
    SpinBoxProps sp;
    ex.get("value", sp.value);
    ex.get("arrowsWidth", sp.arrowsWidth);
    ex.get("arrowColor", sp.arrowColor);
    ex.get("arrowHoverColor", sp.arrowHoverColor);
    ex.get("placeholder", sp.placeholder);
    ex.get("fontSize", sp.fontSize);
    ex.get("textColor", sp.textColor);
    ex.get("placeholderColor", sp.placeholderColor);
    ex.get("cursorColor", sp.cursorColor);
    ex.get("focusedBorderColor", sp.focusedBorderColor);
    ex.get("readOnly", sp.readOnly);
    // min/max/step 为 optional, 用 ex.has 模式 (与 Input 数字模式一致)
    if (ex.has("min"))  sp.min  = ex.raw().getProperty("min").toFloat();
    if (ex.has("max"))  sp.max  = ex.raw().getProperty("max").toFloat();
    if (ex.has("step")) sp.step = ex.raw().getProperty("step").toFloat();
    return sp;
}