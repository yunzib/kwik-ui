// ============================================================================
// keyboard.cpp — 虚拟键盘 Keyboard 实现
//
// 三布局键表 + 命中数学 + 文字绘制（复用 TextRenderPipeline）+ RawEvent 注入。
// ============================================================================

module;

#include <algorithm>
#include <cctype>

module kwik.element.keyboard;

import kwik.element.view;
import kwik.element.layer_view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;    // TextLayoutConfig / LayoutTextAlign / FontId / ShapedGlyph
import kwik.render.text.pipeline; // TextRenderPipeline
import kwik.core.log;
import kwik.event;

import std;

// ── 功能键码常量（与平台无关，Input/TextArea 已硬编码消费）──────────────
namespace {
constexpr uint32_t VK_BACK = 0x08;     // Backspace
constexpr uint32_t VK_ENTER = 0x0D;    // Enter
constexpr uint32_t VK_LEFT = 0x25;     // ←
constexpr uint32_t VK_RIGHT = 0x27;    // →
constexpr uint32_t VK_HOME = 0x24;     // Home
constexpr uint32_t VK_END = 0x23;      // End
constexpr uint32_t VK_UP = 0x26;       // ↑（TextArea 用）
constexpr uint32_t VK_DOWN = 0x28;     // ↓（TextArea 用）
}    // namespace

// ============================================================================
// 构造 / 析构
// ============================================================================
Keyboard::Keyboard(ViewProps vp, KeyboardProps kp) : View(std::move(vp)), kp_(std::move(kp)) {
    if (kp_.visible) activate();
    // 订阅焦点变化（失焦自动关闭）；v1 单实例
    if (!s_activeInstance) {
        s_activeInstance = this;
        setFocusChangeHook([](EventTarget *f) { Keyboard::onGlobalFocusChange(f); });
    } else {
        Log::warn("[Keyboard] 多实例: 仅最后一个订阅焦点变化");
    }
}

Keyboard::~Keyboard() {
    if (registered_) deactivate();
    if (s_activeInstance == this) {
        s_activeInstance = nullptr;
        setFocusChangeHook(FocusChangeHook{});    // 清空钩子
    }
}

// ============================================================================
// 图层 activate/deactivate（镜像 MenuView：registerLayerView + drawnElsewhere_）
// ============================================================================
void Keyboard::activate() {
    if (registered_) return;
    registered_ = true;
    drawnElsewhere_ = true;
    LayerStack::instance().registerLayerView(this);
    props.visible = true;    // ← 同步 ViewProps.visible，否则 View::draw 第 199 行直接退场
    frame = panelRect();
    markAllDirty();
}

void Keyboard::deactivate() {
    if (!registered_) return;
    registered_ = false;
    LayerStack::instance().unregisterLayerView(this);
    drawnElsewhere_ = false;
    props.visible = false;    // ← 同步隐蔽态
    if (auto *base = LayerStack::instance().base()) {
        View *root = base;
        while (root->parent()) root = root->parent();
        root->markAllDirty();
    }
    clearAllDirtySubtree();
}

// ============================================================================
// applyKeyboardProps — reconcile 同步入参（visible / layout 变化）
// ============================================================================
void Keyboard::applyKeyboardProps(const KeyboardProps &kp) {
    bool visChanged = (kp_.visible != kp.visible);
    bool layoutChanged = (kp_.layout != kp.layout);
    kp_ = kp;
    if (visChanged) {
        if (kp_.visible)
            activate();
        else
            deactivate();
    }
    if (layoutChanged) {
        shiftSticky_ = false;
        if (registered_) frame = panelRect();    // ← 补：切布局重算面板高（仍需 markDirty 触重绘）
        markDirty();
    }
}

// ============================================================================
// 三布局键表
// ============================================================================
std::vector<std::vector<Keyboard::KeyDef>> Keyboard::keyRows() const {
    switch (kp_.layout) {
    case KeyboardLayout::Text: {
        // 行1: q w e r t y u i o p (10)
        // 行2: a s d f g h j k l    (9)
        // 行3: shift + z x c v b n m + backspace
        // 行4: 123(→symbol) + space(weight 5) + enter(weight 2)
        std::vector<KeyDef> r1;
        for (char c : std::string("qwertyuiop")) r1.push_back({(uint32_t)c, 0, std::string(1, c), 1.0f, false, false});
        std::vector<KeyDef> r2;
        for (char c : std::string("asdfghjkl")) r2.push_back({(uint32_t)c, 0, std::string(1, c), 1.0f, false, false});
        std::vector<KeyDef> r3;
        r3.push_back({0, 0, "shift", 1.5f, true, false});    // shift sticky
        for (char c : std::string("zxcvbnm")) r3.push_back({(uint32_t)c, 0, std::string(1, c), 1.0f, false, false});
        r3.push_back({0, VK_BACK, "Backspace", 1.5f, false, false});
        std::vector<KeyDef> r4;
        r4.push_back({0, 0, "123", 1.5f, false, true});    // → symbol
        r4.push_back({' ', 0, "space", 5.0f, false, false});
        r4.push_back({0, VK_ENTER, "Enter", 2.0f, false, false});
        return {r1, r2, r3, r4};
    }
    case KeyboardLayout::Number: {
        // 行1: 1 2 3 / 行2: 4 5 6 / 行3: 7 8 9 / 行4: . 0 ⌫
        std::vector<KeyDef> r1, r2, r3, r4;
        for (char c : std::string("123")) r1.push_back({(uint32_t)c, 0, std::string(1, c), 1.0f, false, false});
        for (char c : std::string("456")) r2.push_back({(uint32_t)c, 0, std::string(1, c), 1.0f, false, false});
        for (char c : std::string("789")) r3.push_back({(uint32_t)c, 0, std::string(1, c), 1.0f, false, false});
        r4.push_back({'.', 0, ".", 1.0f, false, false});
        r4.push_back({'0', 0, "0", 1.0f, false, false});
        r4.push_back({0, VK_BACK, "Backspace", 1.0f, false, false});
        return {r1, r2, r3, r4};
    }
    case KeyboardLayout::Symbol: {
        // 行1: ! @ # $ % ^ & * ( )
        // 行2: - = + [ ] { } ; :
        // 行3: " , . < > / ? \ |
        // 行4: abc(→text) + space(weight 5) + enter(weight 2)
        std::vector<KeyDef> r1, r2, r3, r4;
        for (char c : std::string("!@#$%^&*()")) r1.push_back({(uint32_t)c, 0, std::string(1, c), 1.0f, false, false});
        for (char c : std::string("-+=[]{};:")) r2.push_back({(uint32_t)c, 0, std::string(1, c), 1.0f, false, false});
        for (char c : std::string("\",.<>/?\\|")) r3.push_back({(uint32_t)c, 0, std::string(1, c), 1.0f, false, false});
        r4.push_back({0, 0, "abc", 1.5f, false, true});    // → text
        r4.push_back({' ', 0, "space", 2.0f, false, false});
        r4.push_back({0, VK_ENTER, "Enter", 2.0f, false, false});
        return {r1, r2, r3, r4};
    }
    }
    return {};
}

// ============================================================================
// 几何
// ============================================================================
float Keyboard::panelHeight() const {
    auto rows = keyRows();
    return (float)rows.size() * kp_.keyHeight + ((float)rows.size() - 1) * kp_.keyGap + kp_.panelPadding.vertical();
}

Rect Keyboard::panelRect() const {
    auto *base = LayerStack::instance().base();
    float w = base ? base->frame.width : 0;
    float h = base ? base->frame.height : 0;
    float ph = panelHeight();
    return Rect{0, h - ph, w, ph};    // 视口底部全宽
}

// ============================================================================
// 测量 / 布局（base 树中占 0，层自管 frame = panelRect）
// ============================================================================
Size Keyboard::onMeasure(Constraints constraints) {
    return constraints.constrain(Size{0, 0});    // 不占 base 空间
}

void Keyboard::onLayout() {
    if (registered_) frame = panelRect();    // dock 底部
}

// ============================================================================
// 键命中：按布局几何逆推键
// ============================================================================
const Keyboard::KeyDef *Keyboard::hitKey(Point local, Rect &keyRectOut) {
    auto rows = keyRows();
    const float availW = frame.width - kp_.panelPadding.horizontal();
    const float innerX = frame.x + kp_.panelPadding.left;
    const float innerY = frame.y + kp_.panelPadding.top;
    for (size_t r = 0; r < rows.size(); ++r) {
        const auto &row = rows[r];
        float totalWeight = 0;
        for (auto &k : row) totalWeight += k.weight;
        float unitW = (availW - ((float)row.size() - 1) * kp_.keyGap) / totalWeight;
        float x = innerX;
        float y = innerY + r * (kp_.keyHeight + kp_.keyGap);
        for (auto const &k : row) {
            float w = k.weight * unitW;
            Rect kr{x, y, w, kp_.keyHeight};
            if (kr.contains(local)) {
                keyRectOut = kr;
                return &k;
            }
            x += w + kp_.keyGap;
        }
    }
    return nullptr;    // 命中间隙或面板空白区
}

// ============================================================================
// 合成 RawEvent 并注入（可打印键 TextInput / 功能键 KeyDown）
// ============================================================================
void Keyboard::injectKey(const KeyDef &key) {
    const auto &inj = rawEventInjector();
    if (!inj) return;
    RawEvent raw{};
    raw.device = RawEvent::Device::Keyboard;
    if (key.charCode != 0) {
        uint32_t cp = key.charCode;
        if (shiftSticky_ && cp < 0x80) cp = (uint32_t)std::toupper((unsigned char)cp);
        raw.action = RawEvent::Action::TextInput;
        raw.charCode = cp;
    } else if (key.keyCode != 0) {
        raw.action = RawEvent::Action::KeyDown;
        raw.keyCode = key.keyCode;
    }
    inj(raw);    // → Application 注册的 eventRouter_.feedRawEvent
}

// ============================================================================
// hitTest — 面板内返 this 吞点（LayerStack 顶→底），面板外穿透 base
// ============================================================================
EventTarget *Keyboard::hitTest(Point p) {
    if (!props.visible || !registered_ || !frame.contains(p)) return nullptr;
    return this;
}

// ============================================================================
// onEvent — Tap=命中键并注入；ESC=关闭（同 MenuView）
// ============================================================================
bool Keyboard::handleTap(Point local) {
    Rect kr;
    const KeyDef *k = hitKey(local, kr);
    if (!k) return false;    // 命中间隙，不吞点

    // shift sticky 切换（不注入字符）
    if (k->isShift) {
        shiftSticky_ = !shiftSticky_;
        markDirty();
        return true;
    }
    // 布局切换键（123→symbol / abc→text）
    if (k->isLayoutSwitch) {
        if (k->label == "123")
            kp_.layout = KeyboardLayout::Symbol;
        else if (k->label == "abc")
            kp_.layout = KeyboardLayout::Text;
        shiftSticky_ = false;
        markDirty();
        return true;
    }
    // 数字/字母/标点/功能键 → 同路径自动注入 + 旁路 onKey 通知
    injectKey(*k);                                          // ← 保留：物理键盘同路径，退格/光标/中文全对
    if (handlers.onKey) handlers.onKey(makeKeyArgs(*k));    // ← 旁路给 JS 感知
    // text 布局：shift 注入一次后自动撤销（大写首字符语义）
    if (shiftSticky_ && kp_.layout == KeyboardLayout::Text) { shiftSticky_ = false; }
    markDirty();    // 高亮反馈（一次按下键色变化）
    return true;
}

bool Keyboard::onEvent(const DispatchEvent &event) {
    if (!registered_) return false;
    if (event.type == DispatchEvent::Type::Tap) { return handleTap(Point{event.globalX, event.globalY}); }
    if (event.type == DispatchEvent::Type::KeyAction && event.keyCode == 27) {
        // ESC 关键盘（无聚焦控件时经 hitTest(0,0) 到达本层）
        kp_.visible = false;
        deactivate();
        return true;
    }
    return false;
}

// ============================================================================
// draw — 未注册层时空转清脏（镜像 MenuView，防主循环空转）
// ============================================================================
void Keyboard::draw(Graphics &g) {
    if (!registered_) {
        clearAllDirtySubtree();
        return;
    }
    View::draw(g);
}

// ============================================================================
// onDraw — 面板背景 + 逐键圆角矩形 + UTF-8 文字（TextRenderPipeline）
// ============================================================================
void Keyboard::onDraw(Graphics &g) {
    if (!props.visible || !registered_) return;

    // ① 面板背景 + 裁剪
    g.save();
    g.drawRoundedRect(frame, kp_.panelRadius, kp_.background);
    g.clipRoundedRect(frame, kp_.panelRadius);

    auto rows = keyRows();
    const float availW = frame.width - kp_.panelPadding.horizontal();
    const float innerX = frame.x + kp_.panelPadding.left;
    const float innerY = frame.y + kp_.panelPadding.top;

    auto &pipe = TextRenderPipeline::instance();
    FontId fid = pipe.activeFont();
    TextLayoutConfig cfg;
    cfg.maxWidth = 1e10f;    // 不限宽（单字符/标签居中即可）
    cfg.align = LayoutTextAlign::Start;

    for (size_t r = 0; r < rows.size(); ++r) {
        const auto &row = rows[r];
        float totalWeight = 0;
        for (auto const &k : row) totalWeight += k.weight;
        float unitW = (availW - ((float)row.size() - 1) * kp_.keyGap) / totalWeight;
        float x = innerX;
        float y = innerY + (float)r * (kp_.keyHeight + kp_.keyGap);
        for (auto const &k : row) {
            float w = k.weight * unitW;
            Rect kr{x, y, w, kp_.keyHeight};
            // 背景：shift sticky / 功能键 / 布局切换键 用高亮底
            Color bg = kp_.keyBackground;
            if ((k.isShift && shiftSticky_) || k.keyCode != 0 || k.isLayoutSwitch) bg = kp_.keyActiveBackground;
            g.drawRoundedRect(kr, kp_.keyRadius, bg);

            // 键面文字（shift sticky 时单字符转大写）
            std::string label = k.label;
            if (k.charCode != 0 && label.size() == 1 && shiftSticky_)
                label[0] = (char)std::toupper((unsigned char)label[0]);
            if (!label.empty()) {
                auto result = pipe.layoutText(label, fid, kp_.keyFontSize, cfg);
                pipe.ensureGlyphs(*result);
                // Log::info("[KB] '{}' glyphs={} totalW={:.1f} totalH={:.1f}", label, result->glyphs.size(),
                //           result->totalWidth, result->totalHeight);
                float tx = kr.x + (kr.width - result->totalWidth) * 0.5f;
                float ty = kr.y + (kr.height - result->totalHeight) * 0.5f;
                g.save();
                g.translate(tx, ty);
                g.drawTextCached(result->glyphs, kp_.keyTextColor);
                g.restore();
            }
            x += w + kp_.keyGap;
        }
    }
    g.restore();    // 解除面板裁剪
}

// ============================================================================
// getProperty / setProperty — getProp/setProp（visible / layout）
// ============================================================================
std::string Keyboard::getProperty(const char *name) const {
    std::string n{name};
    if (n == "visible") return kp_.visible ? "true" : "false";
    if (n == "layout") {
        switch (kp_.layout) {
        case KeyboardLayout::Text: return "text";
        case KeyboardLayout::Number: return "number";
        case KeyboardLayout::Symbol: return "symbol";
        }
    }
    return View::getProperty(name);
}

bool Keyboard::setProperty(const char *name, const char *value) {
    std::string n{name};
    if (n == "visible") {
        bool v = (value && (value[0] == 't' || value[0] == '1'));
        if (kp_.visible != v) {
            kp_.visible = v;
            if (v)
                activate();
            else
                deactivate();
        }
        return true;
    }
    if (n == "layout") {
        if (std::string(value) == "text")
            kp_.layout = KeyboardLayout::Text;
        else if (std::string(value) == "number")
            kp_.layout = KeyboardLayout::Number;
        else if (std::string(value) == "symbol")
            kp_.layout = KeyboardLayout::Symbol;
        else
            return false;
        shiftSticky_ = false;
        markDirty();
        return true;
    }
    return View::setProperty(name, value);
}

KeyArgs Keyboard::makeKeyArgs(const KeyDef &key) const {
    KeyArgs a;
    a.charCode = key.charCode;
    a.keyCode = key.keyCode;
    if (key.charCode != 0) {
        uint32_t cp = key.charCode;
        if (shiftSticky_ && cp < 0x80) cp = (uint32_t)std::toupper((unsigned char)cp);
        // UTF-8 编码（与 Input 文本存储一致，避免 JS 侧多字节错乱）
        if (cp < 0x80) {
            a.value += (char)cp;
        } else if (cp < 0x800) {
            a.value += (char)(0xC0 | (cp >> 6));
            a.value += (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            a.value += (char)(0xE0 | (cp >> 12));
            a.value += (char)(0x80 | ((cp >> 6) & 0x3F));
            a.value += (char)(0x80 | (cp & 0x3F));
        } else {
            a.value += (char)(0xF0 | (cp >> 18));
            a.value += (char)(0x80 | ((cp >> 12) & 0x3F));
            a.value += (char)(0x80 | ((cp >> 6) & 0x3F));
            a.value += (char)(0x80 | (cp & 0x3F));
        }
    }
    return a;
}

Keyboard *Keyboard::s_activeInstance = nullptr;

void Keyboard::onGlobalFocusChange(EventTarget *focused) {
    auto *kb = s_activeInstance;
    if (!kb || !kb->registered_) return;                      // 键盘未打开，不处理
    bool isTextInput = focused && focused->acceptsFocus();    // Input/TextArea 保留，其它失焦关闭
    if (!isTextInput) {
        kb->kp_.visible = false;
        kb->deactivate();
    }
}