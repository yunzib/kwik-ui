// ============================================================================
// keyboard.cppm — 虚拟键盘 Keyboard（浮层 OSK）
//
// 复用物理键盘整条输入管线：按键 → 合成 RawEvent{device=Keyboard} →
// 本树 EventRouter（树服务槽 kSvcEventRouter 直取，多树各回各家）→
// KeyboardHandler（KeyDown→KeyAction / TextInput→CharInput）→
// focusManager_.focused() 设 presetTarget → Input/TextArea::onEvent 消费。
// 行为与物理键盘完全一致。
//
// 浮层化（镜像 LayerView/MenuView）：
//   visible=true → LayerStack::registerLayerView + drawnElsewhere_=true
//   （base 树跳过绘制/命中；LayerStack hitTest 顶→底，面板命中吞点、面板外穿透）
//   位置三件套（键位锚定批）：dock="bottom"（dock 视口底部，widthRatio 比宽
//   水平居中 + offsetBottom 上抬）| dock="floating"（x/y 绝对锚定），
//   面板高度 = 每行 keyHeight × 行高缩放（工具行 0.6）+ panelPadding。
//   visible=false → 反向（图层注销 + base 重绘 + 清脏防 ghost），节点保留在树。
//
// 失焦自动收（会话中介）：activate 期间订阅本树 TextInputSession（槽
// kSvcInputSession）——快照落 nullopt（焦点离开编辑器/无焦点）即收键盘。
// 键盘不认识编辑器类型（会话契约见 kwik.core.input_session）。
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

    /** @brief 增量 reconcile 同步：visible → activate/deactivate；layout/位置 → 重绘重排 */
    void applyKeyboardProps(const KeyboardProps &kp);

     /**
     * @brief 属性写入唯一虚入口
     *
     * 命令式路径与 State 增量路径均汇入此处；
     * string 分支=setProp 包装，原生分支=notify 直传。
     */
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    void onDraw(Graphics &g) override;
    EventTarget *hitTest(Point p) override;
    bool onEvent(const DispatchEvent &event) override;
    /** H6：ESC 显式查询——键盘层注册为图层（可见态）才消费 ESC */
    bool acceptsEscape() const override { return registered_; }
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

    /// 键行定义（键表能力批次）：keys + 行高缩放。工具行（光标/编辑键）
    /// 独立降高 heightScale≈0.6×keyHeight，与主键行视觉分层（系统键盘惯例）
    struct KeyRow {
        std::vector<KeyDef> keys;
        float heightScale = 1.0f;    // 行高 = keyHeight × heightScale
    };

    KeyboardProps kp_;
    bool registered_ = false;     // 是否已注册为图层
    bool pendingActivate_ = false;    // 挂树前 visible=true 的延迟激活（onLayout 补挂）
    bool shiftSticky_ = false;    // text 布局 shift 粘滞态（大写切换）

    /// 会话订阅令牌（0=未订阅）。activate 期间收焦点快照、deactivate 退订
    /// ——每实例各自订阅（旧全局单 hook 的多实例/多树缺陷就此消除）
    std::size_t sesTok_ = 0;
    /// 订阅挂靠的会话对象（退订寻址；随 activate/deactivate 成对更新）
    void *sesHost_ = nullptr;

    /// 按下态（键表能力批次，down 驱动）：PointerDown 即注入并标记按下
    /// （视觉反馈早一步），PointerUp/Cancel/失活/析构遣散；Tap 仅吞不发防双发
    std::optional<KeyDef> pressKey_;      // 当前按下的键（拷贝自临时键表行，悬空安全)
    bool pressActive_ = false;            // 按下态在效（pressKey_ 有值且未抬起）
    uint32_t repeatTimerId_ = 0;          // 长按连发 CoreTimer 令牌（0=无）
    uint64_t pressStartMs_ = 0;           // 按下时刻（首迟计时基准）
    uint64_t lastRepeatMs_ = 0;           // 上一次连发时刻（节拍对齐）
    bool firstWait_ = false;              // 首迟窗口期（350ms 未到不发）

    /// 当前布局的键行（每行一组 KeyDef，工具行在前）
    std::vector<KeyRow> layoutRows() const;
    /// 面板高度 = 每行行高（keyHeight × heightScale）+ panelPadding
    float panelHeight() const;
    /// 面板矩形（位置三件套：bottom=dock 底部居中+offsetBottom；floating=x/y 锚定）
    Rect panelRect() const;
    /// 命中第几行/列的哪个键（返回非空 + 写出 keyRect）
    const KeyDef *hitKey(Point local, Rect &keyRectOut);
    /// 给定键，合成 RawEvent 注入
    void injectKey(const KeyDef &key);
    /// 按键触发共用（注入 + onKey 旁路 + shift/布局切换特判）——Down 首发与连发同源
    void triggerKey(const KeyDef &key);
    /// PointerDown 入口：标记按下态 + act-on-down 首发 + 起连发定时器
    bool handlePress(Point local);
    /// Tab 入口：仅吞不发（注入已在 PointerDown 发生，防双发）
    bool handleTap(Point local);
    /// 遣散按下态（抬起/取消/失活共用）：连发定时器 + 按下标记清零
    void endPress();
    /// 连发节拍回调（CoreTimer 每 60ms）：首迟 350ms 过后逐拍注入
    void onRepeatTick();
    /// activate/deactivate（镜像 MenuView）
    void activate();
    void deactivate();
    /// 由 KeyDef 构造 JS onKey 负载（value 含 shift 大写；功能键 value 为空）
    KeyArgs makeKeyArgs(const KeyDef &key) const;
};