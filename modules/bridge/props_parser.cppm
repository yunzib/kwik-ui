module;

export module kwik.bridge.props_parser;

import kwik.core.types;
import kwik.element.props;
import kwik.engine.js_value;
import std;

/**
 * @brief 解析边距
 *
 * 支持格式：
 * - 数值：四边相同
 * - 数组[1]：四边相同
 * - 数组[2]：[水平, 垂直]
 * - 数组[4]：[上, 右, 下, 左]
 * - 对象：{left, top, right, bottom}
 */
export EdgeInsets parseEdgeInsets(const JSValueRef &value);
/**
 * @brief 解析阴影字符串
 *
 * 格式："offsetX offsetY blurRadius color"
 * 示例："0 2px 8px rgba(0,0,0,0.1)"
 */
export Shadow parseShadow(const std::string &str);
/**
 * @brief 解析边框样式
 */
export BorderStyle parseBorderStyle(const std::string &str);

/**
 * @brief 解析框架级属性 (display + child layout)
 */
export ViewProps parseViewProps(const JSValueRef &props);
/**
 * @brief 解析文字内容属性
 */
export TextContent parseTextContent(const JSValueRef &props);
/**
 * @brief 解析按钮交互属性
 */
export ButtonStateProps parseButtonState(const JSValueRef &props);
/**
 * @brief 解析容器布局属性
 */
export ContainerProps parseContainerProps(const JSValueRef &props);

/**
 * @brief 解析图像属性
 *
 * 支持字段:
 *   - src:       文件路径 (string)
 *   - fit:       填充模式 ("fill"|"contain"|"cover"|"none")
 *   - opacity:   图像透明度 (float)
 *   - data:      像素缓冲区 (ArrayBuffer)
 *   - width:     缓冲区宽度 (int, data 模式时必填)
 *   - height:    缓冲区高度 (int, data 模式时必填)
 */
export ImageProps parseImageProps(const JSValueRef &props);

export InputProps parseInputProps(const JSValueRef &props);

export RadioButtonProps parseRadioButtonProps(const JSValueRef &props);

export RadioGroupProps parseRadioGroupProps(const JSValueRef &props);

export CheckboxProps parseCheckboxProps(const JSValueRef &props);