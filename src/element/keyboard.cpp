// ============================================================================
// keyboard.cpp — 虚拟键盘 Keyboard 实现
//
// 三布局键表（工具行独立降高）+ 命中数学 + 文字绘制（复用 TextRenderPipeline）。
// 注入经本树 EventRouter（树服务槽 kSvcEventRouter）；失焦自动收经本树
// TextInputSession 订阅（槽 kSvcInputSession，activate/deactivate 成对）。
// 按键链路（键表能力批次）：down 驱动——PointerDown 即注入并亮按下态，
// 长按 350ms 后每 60ms 连发（CoreTimer）；Tap 仅吞不发防双发。
// ============================================================================

module;


module kwik.element.keyboard;

import kwik.element.view;
import kwik.element.layer_stack;
import kwik.element.layer_view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.core.input_session;    // TextInputSession（会话订阅，失焦自动收）
import kwik.core.timer;            // CoreTimer（长按连发首迟/节拍）
import kwik.render.graphics;
import kwik.render.text.types;    // TextLayoutConfig / LayoutTextAlign / FontId / ShapedGlyph
import kwik.render.text.pipeline; // TextRenderPipeline
import kwik.core.log;
import kwik.event;

import std;

// ── 功能键码常量（与平台无关，Input/TextArea 已硬编码消费）──────────────
namespace {
constexpr std::uint32_t VK_BACK = 0x08;     // Backspace
constexpr std::uint32_t VK_ENTER = 0x0D;    // Enter
constexpr std::uint32_t VK_LEFT = 0x25;     // ←
constexpr std::uint32_t VK_RIGHT = 0x27;    // →
constexpr std::uint32_t VK_HOME = 0x24;     // Home
constexpr std::uint32_t VK_END = 0x23;      // End
constexpr std::uint32_t VK_UP = 0x26;       // ↑（TextArea 用）
constexpr std::uint32_t VK_DOWN = 0x28;     // ↓（TextArea 用）
constexpr std::uint32_t VK_DELETE = 0x2E;   // Delete（Input/TextArea 删后向）
// 修饰键位（与 RawEvent.modifiers 同口径：bit0=Ctrl / bit1=Shift）
constexpr std::uint32_t MOD_SHIFT = 0x2;
// 长按连发节拍（键表能力批次）：首迟 350ms 起发，此后每 60ms 一发
constexpr std::uint64_t kRepeatFirstDelayMs = 350;
constexpr std::uint64_t kRepeatPeriodMs = 60;

/// 取 steady 时钟毫秒（连发计时基准；仅相对差值有意义）
std::uint64_t steadyMs() {
    return (std::uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

/// float → 最短文本（getProp 回读口径，double 用 to_chars 最短往返表示；
/// 与 SpinBox::fmtNum 同算法，避免跨组件拉私有符号）
std::string fmtNum(float v) {
    char buf[64];
    auto res = std::to_chars(buf, buf + sizeof(buf), (double)v);
    return std::string(buf, res.ptr);
}

/// 该键是否参与长按连发（shift/布局切换是状态键，非输入键——不连发）。
/// 用裸 bool 形参：KeyDef 是 Keyboard 私有嵌套类型，自由函数不可命名
bool repeatableKey(bool isShift, bool isLayoutSwitch) { return !isShift && !isLayoutSwitch; }
}    // namespace

// ============================================================================
// 构造 / 析构
// ============================================================================
Keyboard::Keyboard(ViewProps vp, KeyboardProps kp) : View(std::move(vp)), kp_(std::move(kp)) {
    if (kp_.visible) activate();
    // 失焦自动收改为 activate 期间的 TextInputSession 订阅（见 activate），
    // 无构造期全局 hook 注册——多实例/多树均各自成对订阅退订
}

Keyboard::~Keyboard() {
    // dispose 路径：registered_ 时 deactivate 负责退订会话 + 图层硬注销；
    // 未注册（含 pendingActivate 挂树前）无会话订阅，无需善后
    // 连发定时器遣散在先（回调捕获 this，析构后不得再触发）
    if (repeatTimerId_ != 0) CoreTimer::clear(repeatTimerId_);
    if (registered_) deactivate();
}

// ============================================================================
// 图层 activate/deactivate（镜像 MenuView：registerLayerView + drawnElsewhere_）
// ============================================================================
void Keyboard::activate() {
    if (registered_) return;
    // 未挂树（构造期/解析期）：根无层树接线，layersOf 为空——延迟到首次
    // onLayout（父 layout 驱动，此刻接线有效）再注册，防构造期空解引用
    if (!layersOf(this)) {
        pendingActivate_ = true;
        return;
    }
    registered_ = true;
    drawnElsewhere_ = true;
    layersOf(this)->registerLayerView(this);
    props.visible = true;    // ← 同步 ViewProps.visible，否则 View::draw 第 199 行直接退场
    frame = panelRect();
    // 会话订阅（失焦自动收）：焦点快照落 nullopt（非编辑器/无焦点）即收键盘。
    // 快照有值（聚焦另一 Input/TextArea）不动——对焦文本框键盘保持，同旧链路
    if (sesTok_ == 0) {
        if (auto *sess = static_cast<TextInputSession *>(treeService(View::kSvcInputSession))) {
            sesHost_ = sess;
            sesTok_ = sess->addListener([this](const TextInputSession::Snapshot &snap) {
                if (!registered_) return;    // 通知重入防护（本回调即收键盘的来源）
                if (!snap) {
                    kp_.visible = false;
                    deactivate();
                }
            });
        }
    }
    markAllDirty();
}

void Keyboard::deactivate() {
    pendingActivate_ = false;    // 显式关闭撤销延迟激活（挂树前 visible true→false）
    endPress();                  // 按下中失活（会话回调收键盘/ESC 关闭）：连发与按下态一并遣散
    // 会话退订（先于 registered_ 判空：幂等收口，dtor 路径若有残留订阅同样摘除）
    if (sesTok_ != 0) {
        if (auto *sess = static_cast<TextInputSession *>(sesHost_)) sess->removeListener(sesTok_);
        sesTok_ = 0;
        sesHost_ = nullptr;
    }
    if (!registered_) return;
    registered_ = false;
    // 根未接线（teardown 解除后的树析构期）时 layersOf 为空，跳过注销与
    // base 脏标记（层列表已 clear，无残留可清）
    LayerStack *ls = layersOf(this);
    if (ls) ls->unregisterLayerView(this);
    drawnElsewhere_ = false;
    props.visible = false;    // ← 同步隐蔽态
    if (ls) {
        if (auto *base = ls->base()) {
            View *root = base;
            while (root->parent()) root = root->parent();
            root->markAllDirty();
        }
    }
    clearAllDirtySubtree();
}

// ============================================================================
// applyKeyboardProps — reconcile 同步入参（visible / layout 变化）
// ============================================================================
void Keyboard::applyKeyboardProps(const KeyboardProps &kp) {
    bool visChanged = (kp_.visible != kp.visible);
    bool layoutChanged = (kp_.layout != kp.layout);
    bool posChanged = (kp_.dock != kp.dock) || (kp_.widthRatio != kp.widthRatio)
        || (kp_.offsetBottom != kp.offsetBottom) || (kp_.x != kp.x) || (kp_.y != kp.y);
    kp_ = kp;
    if (visChanged) {
        if (kp_.visible)
            activate();
        else
            deactivate();
    }
    if (layoutChanged) {
        shiftSticky_ = false;
        endPress();    // 布局重排后旧按下键几何失配，连发随之遣散
        if (registered_) frame = panelRect();    // ← 补：切布局重算面板高（仍需 markDirty 触重绘）
        markDirty();
    }
    if (posChanged) {
        // 位置三件套：面板重排（bottom↔floating / 比例 / 偏移 / 坐标，全由 JS 参数化）
        if (registered_) frame = panelRect();
        markDirty();
    }
}

// ============================================================================
// 三布局键表（键行可携行高缩放——工具行独立降高 ~0.6×keyHeight）
// ============================================================================
std::vector<Keyboard::KeyRow> Keyboard::layoutRows() const {
    switch (kp_.layout) {
    case KeyboardLayout::Text: {
        // 工具行（降高）: ← → ↑ ↓ Home End Del（7 功能键；text/symbol 布局才有）
        // 行1: q w e r t y u i o p (10)
        // 行2: a s d f g h j k l    (9)
        // 行3: shift + z x c v b n m + backspace
        // 行4: 123(→number) + space(weight 5) + enter(weight 2)
        KeyRow tool;
        tool.heightScale = 0.6f;
        tool.keys.push_back({0, VK_LEFT, "←", 1.0f, false, false});
        tool.keys.push_back({0, VK_RIGHT, "→", 1.0f, false, false});
        tool.keys.push_back({0, VK_UP, "↑", 1.0f, false, false});
        tool.keys.push_back({0, VK_DOWN, "↓", 1.0f, false, false});
        tool.keys.push_back({0, VK_HOME, "Home", 1.0f, false, false});
        tool.keys.push_back({0, VK_END, "End", 1.0f, false, false});
        tool.keys.push_back({0, VK_DELETE, "Del", 1.0f, false, false});

        KeyRow r1, r2, r3, r4;
        for (char c : std::string("qwertyuiop")) r1.keys.push_back({(std::uint32_t)c, 0, std::string(1, c), 1.0f, false, false});
        for (char c : std::string("asdfghjkl")) r2.keys.push_back({(std::uint32_t)c, 0, std::string(1, c), 1.0f, false, false});
        r3.keys.push_back({0, 0, "shift", 1.5f, true, false});    // shift sticky
        for (char c : std::string("zxcvbnm")) r3.keys.push_back({(std::uint32_t)c, 0, std::string(1, c), 1.0f, false, false});
        r3.keys.push_back({0, VK_BACK, "Backspace", 1.5f, false, false});
        r4.keys.push_back({0, 0, "123", 1.5f, false, true});    // → number
        r4.keys.push_back({' ', 0, "space", 5.0f, false, false});
        r4.keys.push_back({0, VK_ENTER, "Enter", 2.0f, false, false});
        return {tool, r1, r2, r3, r4};
    }
    case KeyboardLayout::Number: {
        // 行1: 1 2 3 / 行2: 4 5 6 / 行3: 7 8 9
        // 行4: abc(→text) #+=(→symbol) - . 0 ⌫ Enter——前两键为布局回通开关：
        // 无工具行（光标键对数字编辑无意义），但数字页不能困死用户，123 键
        // 从 text 页直达此处后，回 text/symbol 的就近出口置于底行前段。
        KeyRow r1, r2, r3, r4;
        for (char c : std::string("123")) r1.keys.push_back({(std::uint32_t)c, 0, std::string(1, c), 1.0f, false, false});
        for (char c : std::string("456")) r2.keys.push_back({(std::uint32_t)c, 0, std::string(1, c), 1.0f, false, false});
        for (char c : std::string("789")) r3.keys.push_back({(std::uint32_t)c, 0, std::string(1, c), 1.0f, false, false});
        r4.keys.push_back({0, 0, "abc", 1.0f, false, true});    // → text
        r4.keys.push_back({0, 0, "#+=", 1.0f, false, true});    // → symbol
        r4.keys.push_back({'-', 0, "-", 1.0f, false, false});
        r4.keys.push_back({'.', 0, ".", 1.0f, false, false});
        r4.keys.push_back({'0', 0, "0", 1.0f, false, false});
        r4.keys.push_back({0, VK_BACK, "⌫", 1.0f, false, false});
        r4.keys.push_back({0, VK_ENTER, "Enter", 1.25f, false, false});   // 提交键显强
        return {r1, r2, r3, r4};
    }
    case KeyboardLayout::Symbol: {
        // 工具行（降高）: 同 text 布局
        // 行1: ! @ # $ % ^ & * ( )
        // 行2: - = + [ ] { } ; :
        // 行3: " , . < > / ? \ |
        // 行4: abc(→text) + space(weight 2) + enter(weight 2)
        KeyRow tool;
        tool.heightScale = 0.6f;
        tool.keys.push_back({0, VK_LEFT, "←", 1.0f, false, false});
        tool.keys.push_back({0, VK_RIGHT, "→", 1.0f, false, false});
        tool.keys.push_back({0, VK_UP, "↑", 1.0f, false, false});
        tool.keys.push_back({0, VK_DOWN, "↓", 1.0f, false, false});
        tool.keys.push_back({0, VK_HOME, "Home", 1.0f, false, false});
        tool.keys.push_back({0, VK_END, "End", 1.0f, false, false});
        tool.keys.push_back({0, VK_DELETE, "Del", 1.0f, false, false});

        KeyRow r1, r2, r3, r4;
        for (char c : std::string("!@#$%^&*()")) r1.keys.push_back({(std::uint32_t)c, 0, std::string(1, c), 1.0f, false, false});
        for (char c : std::string("-+=[]{};:")) r2.keys.push_back({(std::uint32_t)c, 0, std::string(1, c), 1.0f, false, false});
        for (char c : std::string("\",.<>/?\\|")) r3.keys.push_back({(std::uint32_t)c, 0, std::string(1, c), 1.0f, false, false});
        r4.keys.push_back({0, 0, "abc", 1.5f, false, true});    // → text
        r4.keys.push_back({' ', 0, "space", 2.0f, false, false});
        r4.keys.push_back({0, VK_ENTER, "Enter", 2.0f, false, false});
        return {tool, r1, r2, r3, r4};
    }
    }
    return {};
}

// ============================================================================
// 几何（每行行高 = keyHeight × heightScale，工具行独立降高）
// ============================================================================
float Keyboard::panelHeight() const {
    auto rows = layoutRows();
    float h = kp_.panelPadding.vertical();
    for (std::size_t r = 0; r < rows.size(); ++r) {
        if (r > 0) h += kp_.keyGap;
        h += kp_.keyHeight * rows[r].heightScale;
    }
    return h;
}

Rect Keyboard::panelRect() const {
    auto *base = layersOf(this)->base();
    float w = base ? base->frame.width : 0;
    float h = base ? base->frame.height : 0;
    float ph = panelHeight();
    float pw = kp_.widthRatio * w;    // 比宽两模式同口径（widthRatio 已钳 [0,1]）
    if (kp_.dock == KeyboardDock::Floating) {
        // floating：x/y 绝对锚定（视口坐标系左上角；JS 全权，不自动钳制视口内）
        return Rect{kp_.x, kp_.y, pw, ph};
    }
    // bottom：dock 视口底部 + 水平居中 + offsetBottom 上抬（>=0 已规范化）
    return Rect{(w - pw) * 0.5f, h - ph - kp_.offsetBottom, pw, ph};
}

// ============================================================================
// 测量 / 布局（base 树中占 0，层自管 frame = panelRect）
// ============================================================================
Size Keyboard::onMeasure(Constraints constraints) {
    return constraints.constrain(Size{0, 0});    // 不占 base 空间
}

void Keyboard::onLayout() {
    if (pendingActivate_) {
        pendingActivate_ = false;
        activate();    // 此刻已挂树（onLayout 由父 layout 驱动），接线有效
    }
    if (registered_) frame = panelRect();    // dock 底部
}

// ============================================================================
// 键命中：按布局几何逆推键（逐行行高与 onDraw 同源——不得再手抄第二套）
// ============================================================================
const Keyboard::KeyDef *Keyboard::hitKey(Point local, Rect &keyRectOut) {
    auto rows = layoutRows();
    const float availW = frame.width - kp_.panelPadding.horizontal();
    const float innerX = frame.x + kp_.panelPadding.left;
    float y = frame.y + kp_.panelPadding.top;
    for (std::size_t r = 0; r < rows.size(); ++r) {
        const auto &row = rows[r].keys;
        const float rowH = kp_.keyHeight * rows[r].heightScale;
        float totalWeight = 0;
        for (auto &k : row) totalWeight += k.weight;
        float unitW = (availW - ((float)row.size() - 1) * kp_.keyGap) / totalWeight;
        float x = innerX;
        for (auto const &k : row) {
            float w = k.weight * unitW;
            Rect kr{x, y, w, rowH};
            if (kr.contains(local)) {
                keyRectOut = kr;
                return &k;
            }
            x += w + kp_.keyGap;
        }
        y += rowH + kp_.keyGap;
    }
    return nullptr;    // 命中间隙或面板空白区
}

// ============================================================================
// 合成 RawEvent 并注入本树 EventRouter（可打印键 TextInput / 功能键 KeyDown）
// ============================================================================
void Keyboard::injectKey(const KeyDef &key) {
    // 注入归属本树：经树服务槽 kSvcEventRouter 直取本运行时路由（多树各回
    // 各家，替代旧进程级全局注入器）；与物理键入同源（各窗回调各自 router）
    auto *router = static_cast<EventRouter *>(treeService(View::kSvcEventRouter));
    if (!router) return;
    RawEvent raw{};
    raw.device = RawEvent::Device::Keyboard;
    raw.modifiers = shiftSticky_ ? MOD_SHIFT : 0;    // 修饰键位随注入（bit1=Shift，shift 大写同源）
    if (key.charCode != 0) {
        std::uint32_t cp = key.charCode;
        if (shiftSticky_ && cp < 0x80) cp = (std::uint32_t)std::toupper((unsigned char)cp);
        raw.action = RawEvent::Action::TextInput;
        raw.charCode = cp;
    } else if (key.keyCode != 0) {
        // Enter 特例: KeyAction 无 VK_RETURN 消费方, 统一改发 TextInput '\n'
        // (Input→blur 提交, TextArea→插入换行), 与物理键盘归一化后行为一致
        if (key.keyCode == VK_ENTER) {
            raw.action = RawEvent::Action::TextInput;
            raw.charCode = '\n';
        } else {
            raw.action = RawEvent::Action::KeyDown;
            raw.keyCode = key.keyCode;
        }
    }
    // → 本树 KeyboardHandler（KeyDown/TextInput→KeyAction/CharInput）→ focused 控件
    router->feedRawEvent(raw);
}

// ============================================================================
// hitTest — 面板内返 this 吞点（LayerStack 顶→底），面板外穿透 base
// ============================================================================
EventTarget *Keyboard::hitTest(Point p) {
    if (!props.visible || !registered_ || !frame.contains(p)) return nullptr;
    return this;
}

// ============================================================================
// 按键链路（down 驱动，键表能力批次）：
//   PointerDown = act-on-down 首发（注入 + onKey + 按下态亮起 + 起连发）；
//   Tap 仅吞不发（注入已发生在按下沿，防双发；间隙照旧不吞）；
//   PointerUp/Cancel = 遣散；shift/布局切换是状态键，按下即生效不连发
// ============================================================================

/// 按键触发共用（PointerDown 首发与连发节拍同源走此）
void Keyboard::triggerKey(const KeyDef &k) {
    // shift sticky 切换（不注入字符）
    if (k.isShift) {
        shiftSticky_ = !shiftSticky_;
        markDirty();
        return;
    }
    // 布局切换键（123→number / abc→text / #+=→symbol）
    if (k.isLayoutSwitch) {
        if (k.label == "123")
            kp_.layout = KeyboardLayout::Number;
        else if (k.label == "abc")
            kp_.layout = KeyboardLayout::Text;
        else if (k.label == "#+=")
            kp_.layout = KeyboardLayout::Symbol;
        shiftSticky_ = false;
        markDirty();
        return;
    }
    // 数字/字母/标点/功能键 → 同路径自动注入 + 旁路 onKey 通知
    injectKey(k);                                          // ← 保留：物理键盘同路径，退格/光标/中文全对
    if (handlers.onKey) handlers.onKey(makeKeyArgs(k));    // ← 旁路给 JS 感知
    // text 布局：shift 注入一次后自动撤销（大写首字符语义）
    if (shiftSticky_ && kp_.layout == KeyboardLayout::Text) { shiftSticky_ = false; }
    markDirty();    // 按下高亮（首发时刻与连发逐拍都需要）
}

bool Keyboard::handlePress(Point local) {
    endPress();    // 兜底遣散（多指/取消后重按防御，正常单指序列此处为空操作）
    Rect kr;
    const KeyDef *k = hitKey(local, kr);
    if (!k) return false;    // 命中间隙，不吞点（与旧 Tap 口径一致）

    pressKey_ = *k;          // 拷贝：键表行是临时对象，命中指针悬空于此
    pressActive_ = true;
    firstWait_ = repeatableKey(k->isShift, k->isLayoutSwitch);    // 输入键走首迟→连发；状态键按下即生效
    pressStartMs_ = steadyMs();
    lastRepeatMs_ = 0;
    if (firstWait_ && repeatTimerId_ == 0) {
        // 60ms 节拍定时器：首迟窗口内空转，350ms 后逐拍发键（onRepeatTick 门控）
        repeatTimerId_ = CoreTimer::setInterval((std::uint32_t)kRepeatPeriodMs, [this]() { onRepeatTick(); });
    }
    triggerKey(*k);    // act-on-down 首发注入 + 按下态视觉（markDirty 内含）
    return true;
}

void Keyboard::endPress() {
    if (repeatTimerId_ != 0) {
        CoreTimer::clear(repeatTimerId_);
        repeatTimerId_ = 0;
    }
    if (!pressActive_ && !pressKey_) return;    // 空收口（幂等）
    pressKey_.reset();
    pressActive_ = false;
    firstWait_ = false;
    if (registered_) markDirty();    // 按下高亮消除（未注册/析构路径不触脏）
}

void Keyboard::onRepeatTick() {
    if (!pressActive_ || !pressKey_) {
        // 键已抬起/失活：遣散自身定时器（endPress 兜底之外的保险）
        if (repeatTimerId_ != 0) {
            CoreTimer::clear(repeatTimerId_);
            repeatTimerId_ = 0;
        }
        return;
    }
    std::uint64_t now = steadyMs();
    if (firstWait_) {
        if (now - pressStartMs_ < kRepeatFirstDelayMs) return;    // 首迟未到，静默空转
        firstWait_ = false;
        lastRepeatMs_ = now;
        triggerKey(*pressKey_);    // 迟滞期满首发连发
        return;
    }
    if (now - lastRepeatMs_ < kRepeatPeriodMs) return;    // 与 60ms 节拍对齐（帧游差内不再发）
    lastRepeatMs_ = now;
    triggerKey(*pressKey_);
}

bool Keyboard::handleTap(Point local) {
    // down 驱动后 Tap 只承担"吞点"职责（注入已发生，防双发）；命中间隙
    // 照旧放行穿透 base（点间隙不吞点）
    Rect kr;
    return hitKey(local, kr) != nullptr;
}

bool Keyboard::onEvent(const DispatchEvent &event) {
    if (!registered_) return false;
    switch (event.type) {
    case DispatchEvent::Type::PointerDown:
        return handlePress(Point{event.globalX, event.globalY});
    case DispatchEvent::Type::Tap:
        return handleTap(Point{event.globalX, event.globalY});
    case DispatchEvent::Type::PointerUp:
    case DispatchEvent::Type::PointerCancel:
        if (pressActive_) { endPress(); return true; }
        return false;    // 无按下沿的零散 Up/Cancel（间隙穿透序列）照旧放行
    case DispatchEvent::Type::KeyAction:
        if (event.keyCode == 27) {
            // ESC 关键盘（无聚焦控件时经 hitTest(0,0) 到达本层）
            kp_.visible = false;
            deactivate();
            return true;
        }
        return false;
    default:
        return false;
    }
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

    auto rows = layoutRows();
    const float availW = frame.width - kp_.panelPadding.horizontal();
    const float innerX = frame.x + kp_.panelPadding.left;
    float y = frame.y + kp_.panelPadding.top;

    // 按下态背景（键表能力批次）：对 keyActiveBackground 深化 15% 派生——
    // 常规键（resting=keyBackground）与功能/状态键（resting=keyActiveBackground）
    // 均获得可辨的按压反馈，无需新增 props 色位
    auto pressed = [](std::uint8_t c) { return (std::uint8_t)(c * 0.85f + 0.5f); };
    const Color pressedBg{pressed(kp_.keyActiveBackground.r), pressed(kp_.keyActiveBackground.g),
                          pressed(kp_.keyActiveBackground.b), kp_.keyActiveBackground.a};

    auto &pipe = TextRenderPipeline::instance();
    FontId fid = pipe.activeFont();
    TextLayoutConfig cfg;
    cfg.maxWidth = 1e10f;    // 不限宽（单字符/标签居中即可）
    cfg.align = LayoutTextAlign::Start;

    for (std::size_t r = 0; r < rows.size(); ++r) {
        const auto &row = rows[r].keys;
        const float rowH = kp_.keyHeight * rows[r].heightScale;
        float totalWeight = 0;
        for (auto const &k : row) totalWeight += k.weight;
        float unitW = (availW - ((float)row.size() - 1) * kp_.keyGap) / totalWeight;
        float x = innerX;
        for (auto const &k : row) {
            float w = k.weight * unitW;
            Rect kr{x, y, w, rowH};
            // 背景：shift sticky / 功能键 / 布局切换键 用高亮底；按下键用深化派生底
            Color bg = kp_.keyBackground;
            if ((k.isShift && shiftSticky_) || k.keyCode != 0 || k.isLayoutSwitch) bg = kp_.keyActiveBackground;
            if (pressActive_ && pressKey_ && pressKey_->charCode == k.charCode && pressKey_->keyCode == k.keyCode
                && pressKey_->weight == k.weight && pressKey_->isLayoutSwitch == k.isLayoutSwitch
                && pressKey_->isShift == k.isShift && pressKey_->label == k.label)
                bg = pressedBg;
            g.drawRoundedRect(kr, kp_.keyRadius, bg);

            // 键面文字（shift sticky 时单字符转大写；工具行标签多字节不受影响）
            std::string label = k.label;
            if (k.charCode != 0 && label.size() == 1 && shiftSticky_)
                label[0] = (char)std::toupper((unsigned char)label[0]);
            if (!label.empty()) {
                auto result = pipe.layoutText(label, fid, kp_.keyFontSize, cfg);
                pipe.ensureGlyphs(*result);
                float tx = kr.x + (kr.width - result->totalWidth) * 0.5f;
                float ty = kr.y + (kr.height - result->totalHeight) * 0.5f;
                g.save();
                g.translate(tx, ty);
                g.drawTextCached(result->glyphs, kp_.keyTextColor);
                g.restore();
            }
            x += w + kp_.keyGap;
        }
        y += rowH + kp_.keyGap;
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
    // 位置三件套回读（getProp 与 setProp 对称——控制显式）
    if (n == "dock") {
        switch (kp_.dock) {
        case KeyboardDock::Bottom: return "bottom";
        case KeyboardDock::Floating: return "floating";
        }
    }
    if (n == "widthRatio") return fmtNum(kp_.widthRatio);
    if (n == "offsetBottom") return fmtNum(kp_.offsetBottom);
    if (n == "x") return fmtNum(kp_.x);
    if (n == "y") return fmtNum(kp_.y);
    return View::getProperty(name);
}

// ============================================================================
// setPropertyTyped — 属性写入唯一入口（visible/layout）
// visible 拦截优先于基类通用 visible（activate/deactivate 为键盘专属语义）
// ============================================================================
bool Keyboard::setPropertyTyped(const char *name, const TypedProp &value) {
	std::string n{name};
	if (n == "visible") {
		auto v = typedToBool(value);      // "true"/"t"/"1"、bool、数值均可
		if (!v) { return false; }
		if (kp_.visible != *v) {
			kp_.visible = *v;
			if (*v) { activate(); } else { deactivate(); }
		}
		return true;
	}
	if (n == "layout") {
		auto *s = std::get_if<std::string>(&value);
		if (!s) { return false; }
		if (*s == "text") { kp_.layout = KeyboardLayout::Text; }
		else if (*s == "number") { kp_.layout = KeyboardLayout::Number; }
		else if (*s == "symbol") { kp_.layout = KeyboardLayout::Symbol; }
		else { return false; }
		shiftSticky_ = false;
		markDirty();
		return true;
	}
	// ── 位置三件套（键位锚定批）：offsetBottom/widthRatio/x/y 数值 + dock 模式。
	//    参数规范化在属性行（与解析同口径）；变更即面板重排（bottom↔floating 同向）
	if (n == "offsetBottom" || n == "widthRatio" || n == "x" || n == "y") {
		auto v = typedToFloat(value);    // double/int64/数字串均可（宽容浮点提取）
		if (!v) { return false; }
		if (n == "offsetBottom") { kp_.offsetBottom = std::max(0.0f, *v); }
		else if (n == "widthRatio") { kp_.widthRatio = std::clamp(*v, 0.0f, 1.0f); }
		else if (n == "x") { kp_.x = *v; }
		else { kp_.y = *v; }
		if (registered_) { frame = panelRect(); }
		markDirty();
		return true;
	}
	if (n == "dock") {
		auto *s = std::get_if<std::string>(&value);
		if (!s) { return false; }
		if (*s == "bottom") { kp_.dock = KeyboardDock::Bottom; }
		else if (*s == "floating") { kp_.dock = KeyboardDock::Floating; }
		else { return false; }
		if (registered_) { frame = panelRect(); }
		markDirty();
		return true;
	}
	return View::setPropertyTyped(name, value);
}

KeyArgs Keyboard::makeKeyArgs(const KeyDef &key) const {
    KeyArgs a;
    a.charCode = key.charCode;
    a.keyCode = key.keyCode;
    a.modifiers = shiftSticky_ ? MOD_SHIFT : 0;    // 修饰键位（bit1=Shift）——onKey 载荷与注入同源
    if (key.charCode != 0) {
        std::uint32_t cp = key.charCode;
        if (shiftSticky_ && cp < 0x80) cp = (std::uint32_t)std::toupper((unsigned char)cp);
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

// （旧 s_activeInstance / onGlobalFocusChange 全局焦点钩子已退役：activate/
//   deactivate 成对订退本树 TextInputSession——失焦自动收逻辑等价内联于订阅回调）
