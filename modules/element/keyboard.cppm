// ============================================================================
// keyboard.cppm — 虚拟键盘 Keyboard（浮层 OSK）
//
// 复用物理键盘整条输入管线：按键 → 合成 RawEvent{device=Keyboard} →
// rawEventInjector() → Application 注册的 feedRawEvent → KeyboardHandler
// （KeyDown→KeyAction / TextInput→CharInput）→ focusManager_.focused()
// 设 presetTarget → Input/TextArea::onEvent 消费。行为与物理键盘完全一致。
//
// 浮层化（镜像 LayerView/MenuView）：
//   visible=true → LayerStack::registerLayerView + drawnElsewhere_=true
//   （base 树跳过绘制/命中；LayerStack hitTest 顶→底，面板命中吞点、面板外穿透）
//   dock 视口底部全宽，面板高度 = 行数 × keyHeight + panelPadding。
//   visible=false → 反向（图层注销 + base 重绘 + 清脏防 ghost），节点保留在树。
//
// 命中自管：Layer hitTest 返 this 后子节点收不到 Tap，故 onEvent 内按坐标算键。
// ============================================================================

module;
#include <cstddef>
#include <stdint.h>

export module kwik.element.keyboard;

import kwik.element.view;
import kwik.element.layer_view; // LayerStack::instance()
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.event;

import std;

/**
 * @brief 虚拟键盘（OSK）浮层组件
 *
 * visible JS 手动控制（v1 无 autoShow）；layout 切键面（Text/Number/Symbol）。
 */
export class Keyboard : public View {
public:
    explicit Keyboard(ViewProps vp, KeyboardProps kp = {});
    ~Keyboard() override;

    ElementType type() const override { return ElementType::Keyboard; }

    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;

    /** @brief 增量 reconcile 同步：visible 变化 → activate/deactivate；layout 变化 → 重绘 */
    void applyKeyboardProps(const KeyboardProps &kp);

protected:
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    void onDraw(Graphics &g) override;
    EventTarget *hitTest(Point p) override;
    bool onEvent(const DispatchEvent &event) override;
    void draw(Graphics &g) override;    // 未注册层时空转清脏（镜像 MenuView）

private:
    /// 键定义：可打印键 charCode≠0（按 shift 取大写），功能键 keyCode≠0
    struct KeyDef {
        uint32_t charCode = 0;          // Unicode 码点（可打印键）
        uint32_t keyCode = 0;           // 虚拟键码（功能键）
        std::string label;              // 键面文字
        float weight = 1.0f;            // 宽度权重（space=5.0 / enter=2.0 加宽）
        bool isShift = false;           // shift 切换键（高亮 + sticky 大写）
        bool isLayoutSwitch = false;    // 布局切换键（label 标识目标：abc/123）
    };

    KeyboardProps kp_;
    bool registered_ = false;     // 是否已注册为图层
    bool shiftSticky_ = false;    // text 布局 shift 粘滞态（大写切换）

    /// 当前活动 Keyboard 实例（v1 单实例，后构造覆盖前，日志告警）
    static Keyboard *s_activeInstance;

    /// 当前布局的键行（每行一组 KeyDef）
    std::vector<std::vector<KeyDef>> keyRows() const;
    /// 面板高度 = 行数 × keyHeight + panelPadding
    float panelHeight() const;
    /// 面板矩形（视口底部全宽）
    Rect panelRect() const;
    /// 命中第几行/列的哪个键（返回非空 + 写出 keyRect）
    const KeyDef *hitKey(Point local, Rect &keyRectOut);
    /// 给定键，合成 RawEvent 注入
    void injectKey(const KeyDef &key);
    /// 命中处理共用：Tap→命中键；ESC→关闭
    bool handleTap(Point local);
    /// activate/deactivate（镜像 MenuView）
    void activate();
    void deactivate();
    /// 由 KeyDef 构造 JS onKey 负载（value 含 shift 大写；功能键 value 为空）
    KeyArgs makeKeyArgs(const KeyDef &key) const;

    /// 焦点变化回调（静态）：失焦自动关闭
    static void onGlobalFocusChange(EventTarget *focused);
};