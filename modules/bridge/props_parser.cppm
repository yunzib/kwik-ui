module;

export module kwik.bridge.props_parser;

import kwik.core.types;
import kwik.core.props;
import kwik.element.typed_prop;
import kwik.engine.js_value;
import kwik.element.view;

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
export TextAreaProps parseTextAreaProps(const JSValueRef &props);
export DropdownProps parseDropdownProps(const JSValueRef &props);
export TextViewProps parseTextViewProps(const JSValueRef &props);

// ============================================================================
// 类型转换函数族
// ============================================================================

/**
 * @brief C++ 类型 → PropType 映射（模板默认未定义，仅允许特化类型）
 *
 * PropsExtractor::get<T>() 内部调用此函数获取类型枚举，
 * 写入 TypedPropMap，供增量更新路径查询原始类型。
 */
template <typename T>
constexpr PropType cppToPropType();

template <>
constexpr PropType cppToPropType<bool>() {
    return PropType::Bool;
}
template <>
constexpr PropType cppToPropType<int>() {
    return PropType::Int;
}
template <>
constexpr PropType cppToPropType<float>() {
    return PropType::Float;
}
template <>
constexpr PropType cppToPropType<double>() {
    return PropType::Float;
}
template <>
constexpr PropType cppToPropType<std::string>() {
    return PropType::String;
}
template <>
constexpr PropType cppToPropType<Color>() {
    return PropType::Color;
}

/**
 * @brief JSValueRef → C++ 类型转换（模板默认未定义，仅允许特化类型）
 */
template <typename T>
T convertTo(const JSValueRef &v);

template <>
inline float convertTo<float>(const JSValueRef &v) {
    return v.toFloat();
}
template <>
inline bool convertTo<bool>(const JSValueRef &v) {
    return v.toBool();
}
template <>
inline int convertTo<int>(const JSValueRef &v) {
    return v.toInt();
}
template <>
inline std::string convertTo<std::string>(const JSValueRef &v) {
    return v.toString();
}
template <>
Color convertTo<Color>(const JSValueRef &v);

// ============================================================================
// PropsExtractor — 统一属性提取器，替代散落的 hasProperty+getProperty 调用
// ============================================================================
/**
 * @brief 统一属性提取器
 *
 * 职责：
 *   1. 类型安全读取属性值：ex.get("fontSize", ip.fontSize)
 *   2. 自动检测双向绑定：在 get<T>() 内检测 __bind_{name}Key，
 *      若存在则通过 cppToPropType<T>() 获取类型标识并写入 TypedPropMap
 *   3. 统一枚举解析：ex.getEnum("align", result.align, {...})
 *
 * 设计约束：
 *   - 不拥有 JSValueRef，生命周期由调用方（TypeCreator lambda）保证
 *   - TypedPropMap* 为可选参数，不传递时不记录绑定
 *   - EdgeInsets / Shadow / ArrayBuffer 等复杂类型通过 raw() 降级
 */
export class PropsExtractor {
public:
    /**
     * @brief 构造提取器
     * @param props JS props 对象的引用（由 element_parser 传入）
     * @param meta  TypedPropMap 指针，get<T>() 检测到绑定时写入
     */
    explicit PropsExtractor(const JSValueRef &props, TypedPropMap *meta = nullptr) : props_(props), meta_(meta) {}

    /**
     * @brief 类型安全地读取属性值
     * @param name  属性名（如 "value"、"fontSize"、"background"）
     * @param out   输出引用（支持 float/bool/int/string/Color）
     * @return true  属性存在且值非 null/undefined
     *
     * 内部行为：
     *   1. 检查 hasProperty(name)
     *   2. 检查 __bind_{name}Key 是否存在
     *      存在时记录 PropEntry{type, hasBinding=true} 到 meta_
     *   3. 调用 convertTo<T>() 将 JSValue 转为 C++ 类型
     *
     * 对比旧模式（3行 → 1行）:
     *   旧: if (props.hasProperty("fontSize")) result.fontSize = props.getProperty("fontSize").toFloat();
     *   新: ex.get("fontSize", result.fontSize);
     */
    template <typename T>
    bool get(const char *name, T &out) {
        if (!props_.hasProperty(name)) return false;
        auto val = props_.getProperty(name);
        if (val.isNull() || val.isUndefined()) return false;
        tryRecordBinding(name, cppToPropType<T>());
        out = convertTo<T>(val);
        return true;
    }

    /**
     * @brief 枚举值提取（字符串 → C++ 枚举）
     * @param name    属性名
     * @param out     枚举值输出
     * @param mapping 映射表 [{字符串, 枚举值}]
     * @return true    属性存在且匹配映射表某项
     *
     * 枚举字段在 JS 侧以字符串传递，此处统一做 string→enum 转换。
     * 绑定检测时 typeHint 记录为 PropType::String（因枚举源头是字符串）。
     */
    template <typename E>
    bool getEnum(const char *name, E &out, std::initializer_list<std::pair<const char *, E>> mapping) {
        if (!props_.hasProperty(name)) return false;
        tryRecordBinding(name, PropType::String);
        std::string str = props_.getProperty(name).toString();
        for (auto &[key, val] : mapping) {
            if (str == key) {
                out = val;
                return true;
            }
        }
        return false;
    }

    /**
     * @brief 检查属性是否存在
     */
    bool has(const char *name) const { return props_.hasProperty(name); }

    /**
     * @brief 获取原始 JSValueRef（用于复杂类型解析的降级路径）
     *
     * 使用场景（这些类型无法用 get<T> 一行解决）:
     *   EdgeInsets:  number/array[2]/array[4]/object 多重形态
     *   Shadow:      "offsetX offsetY blurRadius color" 字符串解析
     *   ArrayBuffer: pixels 缓冲区直接操作 JSValue 底层接口
     *
     * 示例:
     *   if (ex.has("padding"))
     *       result.padding = parseEdgeInsets(ex.raw().getProperty("padding"));
     */
    const JSValueRef &raw() const { return props_; }

private:
    const JSValueRef &props_;
    TypedPropMap *meta_;

    /**
     * @brief 检查并记录绑定元数据
     * @param name  属性名
     * @param type  属性原始类型
     *
     * 在 props_ 上检测 __bind_{name}Key 隐藏属性是否存在。
     * （resolveRefProp 在 JS 模块执行时注入了这些隐藏属性）
     * 若存在则写入 meta_，后续 element_parser 通过
     * forEachBinding 遍历此记录来注册 JSStateBinding。
     */
    void tryRecordBinding(const char *name, PropType type) {
        if (!meta_) return;
        std::string key = std::string("__bind_") + name + "Key";
        if (props_.hasProperty(key.c_str())) { meta_->set(name, type, true); }
    }
};

// ============================================================================
// 解析函数声明 — 全部改为 PropsExtractor& 参数
// ============================================================================

/**
 * @brief 解析边距保留 JSValueRef 签名（多重形态解析逻辑不变）
 *
 * 因为 EdgeInsets 需判断 number / array[2] / array[4] / object
 * 四种不同的 JS 值形态，不适合用 get<T> 模板统一处理。
 * parseXxxProps 中通过 ex.raw().getProperty("padding") 调用。
 */
export EdgeInsets parseEdgeInsets(const JSValueRef &value);

export Shadow parseShadow(const std::string &str);
export BorderStyle parseBorderStyle(const std::string &str);

export ViewProps parseViewProps(PropsExtractor &ex);
export TextContent parseTextContent(PropsExtractor &ex);
export ButtonStateProps parseButtonState(PropsExtractor &ex);
export ContainerProps parseContainerProps(PropsExtractor &ex);
export ImageProps parseImageProps(PropsExtractor &ex);
export InputProps parseInputProps(PropsExtractor &ex);
export RadioButtonProps parseRadioButtonProps(PropsExtractor &ex);
export RadioGroupProps parseRadioGroupProps(PropsExtractor &ex);
export CheckboxProps parseCheckboxProps(PropsExtractor &ex);
export TextAreaProps parseTextAreaProps(PropsExtractor &ex);
export DropdownProps parseDropdownProps(PropsExtractor &ex);
export SliderProps parseSliderProps(PropsExtractor &ex);
export ProgressBarProps parseProgressBarProps(PropsExtractor &ex);
export SwitchProps parseSwitchProps(PropsExtractor &ex);
export LineProps parseLineProps(PropsExtractor &ex);
export SpinnerProps parseSpinnerProps(PropsExtractor &ex);
export TableProps parseTableProps(PropsExtractor &ex);
export TextViewProps parseTextViewProps(PropsExtractor &ex);
export TabsProps parseTabsProps(PropsExtractor &ex);
export DialogProps parseDialogProps(PropsExtractor &ex);