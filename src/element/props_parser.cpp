module;

#include <cstddef>

module kwik.element.props_parser;
import kwik.engine.color_parser;
import kwik.core.types;
import kwik.element.props;
import kwik.engine.js_value;


    EdgeInsets parseEdgeInsets(const JSValueRef& value) {
        if (value.isNull() || value.isUndefined()) {
            return EdgeInsets{};
        }
        
        // 数值：四边相同
        if (value.isNumber()) {
            return EdgeInsets(value.toFloat());
        }
        
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
                return EdgeInsets(
                    value.getArrayElement(0).toFloat(),
                    value.getArrayElement(1).toFloat(),
                    value.getArrayElement(2).toFloat(),
                    value.getArrayElement(3).toFloat()
                );
            }
        }
        
        // 对象
        if (value.isObject()) {
            return EdgeInsets(
                value.getProperty("left").toFloat(),
                value.getProperty("top").toFloat(),
                value.getProperty("right").toFloat(),
                value.getProperty("bottom").toFloat()
            );
        }
        
        return EdgeInsets{};
    }
    Shadow parseShadow(const std::string& str) {
        Shadow shadow;
        
        std::istringstream iss(str);
        std::string token;
        std::vector<std::string> parts;
        
        while (iss >> token) {
            parts.push_back(token);
        }
        
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
            for (size_t i = 3; i < parts.size(); i++) {
                colorStr += parts[i];
            }
            shadow.color = parseColor(colorStr);
        }
        
        return shadow;
    }
    BorderStyle parseBorderStyle(const std::string& str) {
        if (str == "solid") return BorderStyle::Solid;
        if (str == "dashed") return BorderStyle::Dashed;
        return BorderStyle::None;
    }
    ViewProps parseViewProps(const JSValueRef& props) {
        ViewProps result;
        
        if (!props.isObject()) {
            return result;
        }
        
        // 尺寸
        if (props.hasProperty("width")) {
            auto w = props.getProperty("width");
            if (!w.isNull() && !w.isUndefined()) {
                result.width = w.toFloat();
            }
        }
        
        if (props.hasProperty("height")) {
            auto h = props.getProperty("height");
            if (!h.isNull() && !h.isUndefined()) {
                result.height = h.toFloat();
            }
        }
        
        // 背景
        if (props.hasProperty("background")) {
            result.background = parseColor(props.getProperty("background").toString());
        }
        
        // 圆角
        if (props.hasProperty("borderRadius")) {
            result.borderRadius = props.getProperty("borderRadius").toFloat();
        }
        
        // 边框
        if (props.hasProperty("borderWidth")) {
            result.borderWidth = props.getProperty("borderWidth").toFloat();
        }
        
        if (props.hasProperty("borderColor")) {
            result.borderColor = parseColor(props.getProperty("borderColor").toString());
        }
        
        if (props.hasProperty("borderStyle")) {
            result.borderStyle = parseBorderStyle(props.getProperty("borderStyle").toString());
        }
        
        // 间距
        if (props.hasProperty("padding")) {
            result.padding = parseEdgeInsets(props.getProperty("padding"));
        }
        
        if (props.hasProperty("margin")) {
            result.margin = parseEdgeInsets(props.getProperty("margin"));
        }
        
        // 可见性
        if (props.hasProperty("visible")) {
            result.visible = props.getProperty("visible").toBool();
        }
        
        // 透明度
        if (props.hasProperty("opacity")) {
            result.opacity = props.getProperty("opacity").toFloat();
        }
        
        // 阴影
        if (props.hasProperty("shadow")) {
            result.shadow = parseShadow(props.getProperty("shadow").toString());
        }
        
        return result;
    }