// ── Button 文档页 ──
// 页契约同 view / state：meta（id / 分组 / 双语 title / desc / 分组属性表）
// + 内容自组装 content()。手稿 = docs/components/button.md。
// 五个示例小节：变体 / 交互状态 / 尺寸与形状 / 点击与 State 联动 /
// 点击消费与冒泡——每节演示紧随代码卡。State 模块顶层创建，跨重建不重置
// （惯例同 view / state 页）。
// 口径订正：JS 侧 onClick「绑定即消费」（适配层调用成功恒视为消费，
// 返回值不参与判定；view.cpp:36 注释同源）——旧文案「返回 true 才消费」
// 仅适用于 C++ std::function 槽位，JS 文档不再沿用。
// 视觉基线：卡内「软面板 (#F8FAFC) + 竖分隔线 + 固定宽结果 pill」节奏，
// 提示文字统一灰色小号随面板底部（对齐 state 页口径）。
import { View, Flex, Text, Button, State } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section, chip } from '../ui/page_kit.js';

export const meta = {
    id: 'button',
    group: 'base',
    title: { zh: 'Button 按钮', en: 'Button' },
    desc: {
        zh: '承载一次点击操作的交互组件：hover/press 视觉状态（显式指定，或从'
            + '背景色自动推导）、按下自动缩放、阴影随悬停升起；文字/圆角/尺寸'
            + '可自定义。绑定 onClick 即消费点击——祖先不再收到本次点击。',
        en: 'An interactive component for click actions: hover/press visual states '
            + '(explicit or auto-derived from the background), built-in press scale, '
            + 'hover-raised shadows; customizable text / radius / size. Binding '
            + 'onClick consumes the tap — ancestors no longer receive it.',
    },
    // 属性表（分组结构）：[{ title: {zh,en}, rows: [[name, type, def, {zh,en}], ...] }]
    props: [
        { title: { zh: '内容与文字', en: 'Content & Type' }, rows: [
            ['text', 'string', "''", { zh: '按钮文字', en: 'Button label' }],
            ['fontSize', 'number', '16', { zh: '文字字号 px', en: 'Font size in px' }],
            ['color', '#rrggbb', '#FFFFFF', { zh: '文字颜色', en: 'Text color' }],
            ['fontWeight', 'normal | medium | bold', 'normal', { zh: '字重', en: 'Font weight' }],
        ]},
        { title: { zh: '交互状态（不设则自动从背景推导）', en: 'Interaction states (auto-derived)' }, rows: [
            ['hoverBackground', '#rrggbb', 'background×0.80', { zh: '悬停背景色', en: 'Hover background' }],
            ['pressedBackground', '#rrggbb', 'background×0.62', { zh: '按下背景色', en: 'Pressed background' }],
            ['hoverBorderColor', '#rrggbb', '-', { zh: '悬停边框色', en: 'Hover border color' }],
            ['pressedBorderColor', '#rrggbb', '-', { zh: '按下边框色', en: 'Pressed border color' }],
            ['hoverShadow', 'string', '-', { zh: '悬停阴影（如 "0 6 16 #2563EB88"）', en: 'Hover shadow, e.g. "0 6 16 #2563EB88"' }],
            ['pressedShadow', 'string', '-', { zh: '按下阴影', en: 'Pressed shadow' }],
            ['pressedScale', 'number', '0.95', { zh: '按下自动缩放（内置生效，无需声明）', en: 'Built-in press scale (no need to declare)' }],
        ]},
        { title: { zh: '事件', en: 'Events' }, rows: [
            ['onClick', '(e) => {...}', '-', { zh: '点击回调。e.x/e.y = 点击点在按钮内的相对坐标（左上角为原点，稳定不变量）；e.gx/e.gy = 窗口坐标双载（随滚动/摆位漂移，窗口级定位用）。绑定即消费——祖先的 onClick 不再收到本次点击', en: 'Tap callback. e.x/e.y = click position inside the button (origin top-left, stable); e.gx/e.gy = window coordinates (dual payload, drifts with scroll/placement, for window-level anchoring). Binding consumes the tap — ancestor onClick never fires' }],
        ]},
    ],
};

// ── 双语词典（小节标题 / 演示标签 / 提示）──
const L = {
    s1: { zh: '四种变体', en: 'Variants' },
    s2: { zh: '交互状态（hover / press）', en: 'Interaction States (hover / press)' },
    s3: { zh: '尺寸与形状', en: 'Size & Shape' },
    s4: { zh: '点击与 State 联动', en: 'Click & State' },
    s5: { zh: '点击消费与冒泡', en: 'Consume & Bubble' },
    // s1
    fBtn: { zh: '主要操作', en: 'Primary' },
    tBtn: { zh: '次要操作', en: 'Tonal' },
    oBtn: { zh: '描边操作', en: 'Outlined' },
    gBtn: { zh: '幽灵操作', en: 'Ghost' },
    fLbl: { zh: '填充', en: 'Filled' },
    tLbl: { zh: '色调', en: 'Tonal' },
    oLbl: { zh: '描边', en: 'Outlined' },
    gLbl: { zh: '幽灵', en: 'Ghost' },
    fMsg: { zh: '点击了「主要操作」', en: 'Clicked "Primary"' },
    tMsg: { zh: '点击了「次要操作」', en: 'Clicked "Tonal"' },
    oMsg: { zh: '点击了「描边操作」', en: 'Clicked "Outlined"' },
    gMsg: { zh: '点击了「幽灵操作」', en: 'Clicked "Ghost"' },
    clickAny: { zh: '点击任意变体按钮', en: 'Click any variant' },
    // s2
    actBtn: { zh: '功能操作', en: 'Action' },
    autoLbl: { zh: '自动推导', en: 'Auto-derived' },
    explicitLbl: { zh: '显式三态', en: 'Explicit states' },
    elevatedLbl: { zh: '悬停阴影', en: 'Hover shadow' },
    autoCap: { zh: '只给背景色，hover/press 自动加深', en: 'background only; auto darken' },
    explicitCap: { zh: '三态显式指定，覆盖推导值', en: 'three states override derivation' },
    elevatedCap: { zh: '默认阴影 + hover 全向晕开', en: 'resting shadow + hover glow' },
    // s3
    lgBtn: { zh: '大按钮 L', en: 'Large' },
    mdBtn: { zh: '中按钮 M', en: 'Medium' },
    smBtn: { zh: '小按钮 S', en: 'Small' },
    r4: { zh: '方角', en: 'radius 4' },
    r8: { zh: '常规', en: 'radius 8' },
    cap: { zh: '胶囊', en: 'capsule' },
    sizeLbl: { zh: '尺寸', en: 'Size' },
    shapeLbl: { zh: '形状', en: 'Shape' },
    // s4
    count: { zh: '点击计数', en: 'Click count' },
    incBtn: { zh: '+1', en: '+1' },
    coordBtn: { zh: '点上取坐标', en: 'Read coordinates' },

    coordWin: { zh: '窗口', en: 'window' },
    resetBtn: { zh: '清零', en: 'Reset' },
    resetMsg: { zh: '计数已清零', en: 'Counter reset' },
    coordFmt: { zh: '点按 @ 按钮内', en: 'Tapped @ inside button' },
    // s5
    plainTxt: { zh: '点我试试冒泡（纯文本无回调）', en: 'Tap me to bubble (plain text, no callback)' },
    innerBtn: { zh: '按钮（绑定即消费）', en: 'Button (bound = consume)' },
    outerCount: { zh: '容器收到冒泡', en: 'Container received bubble' },
    btnCount: { zh: '按钮消费计数', en: 'Button consumption count' },
    s1Hint: {
        zh: '同一组件四种外观：填充（背景=主色）· 色调（浅底深字）· 描边（边框示'
            + '意次级动作）· 幽灵（无背景纯文字）。无底色款必须显式声明 '
            + "background: 'transparent'——不写 background 键的按钮启用主题蓝兜底"
            + '（Material 语义），透明外观会被盖。hover/press 显式声明时覆盖自动'
            + '推导，点击结果回填上方 pill。',
        en: 'One component, four looks: filled (solid) · tonal (light bg / dark text) '
            + '· outlined (bordered) · ghost (text only). Ghost/outlined MUST declare '
            + "background: 'transparent' explicitly — buttons without a background key "
            + 'fall back to the theme primary (Material semantics), which would cover '
            + 'the transparent look. Explicit hover/press overrides auto-derivation; '
            + 'click results land back in the pill above.',
    },
    s2Hint: {
        zh: '左：只给 background——hover×0.80 / press×0.62 自动加深。中：三态显式'
            + '指定，覆盖推导值。右：默认阴影 + hover 全向晕开（色串末两位 HEX 透明'
            + '度；offsetY>0 向下投影——只贴底缘明显是抬升语义，offsetY=0 才四周'
            + '包裹）。按下自动 0.95 缩放，无需声明。',
        en: 'Left: background only — hover / press auto-darken (×0.80 / ×0.62). '
            + 'Middle: all three states explicit (override derivation). Right: resting '
            + 'shadow + omnidirectional hover glow (last 2 hex digits = alpha; offsetY>0 '
            + 'projects downward — bottom-edge-only read is the elevation semantics, '
            + 'offsetY=0 wraps all four edges). Press scale 0.95 is built in.',
    },
    s3Hint: {
        zh: '按钮尺寸 = height + fontSize + padding 组合；形状由 borderRadius 决定，'
            + 'radius ≥ 半高即成胶囊。',
        en: 'Button size = height + fontSize + padding combo; shape = borderRadius, '
            + 'radius ≥ half of height turns capsule.',
    },
    s4Hint: {
        zh: 'onClick 写 State → 整树 reconcile → 右侧数值联动（未绑定键走全路径，'
            + '惯例同 State 页）。e.x/e.y 是点击点在按钮内的相对坐标，左上角原点；'
            + 'e.gx/e.gy 是窗口坐标双载，随滚动漂移。',
        en: 'onClick writes State → tree reconciles → right panel follows (unbound keys '
            + 'take the full path, same as the State page). e.x/e.y are position inside '
            + 'the button, origin top-left; e.gx/e.gy are window coordinates (dual '
            + 'payload, drift with scroll).',
    },
    s5Hint: {
        zh: 'Tap 未被消费则沿祖先链冒泡：点击纯文本/卡片空白 → 冒泡到容器（左计数 '
            + '+1）；点击按钮 → 绑定即消费，容器收不到（右计数 +1，左不动）。JS '
            + '回调层「绑定即消费」——适配层调用成功即视为消费，JS 返回值不参与'
            + '判定（「返回 true 才消费」是 C++ 槽位的规则）。',
        en: 'Unconsumed taps bubble to ancestors: click the plain text / card blank → '
            + 'container fires (left counter +1); click the button → bound = consumed, '
            + 'container never sees it (right +1, left stays). For JS callbacks binding '
            + 'itself consumes — the adapter counts a successful call as consumed; the '
            + 'return value is not inspected ("return true to consume" is the C++ '
            + 'slot rule).',
    },
};

// ── State：全部模块顶层（content() 重跑不重置——惯例同 view / state 页）──
const stVar = new State({ msg: '点击任意变体按钮' });
const stEvt = new State({ n: 0, msg: '点击任意按钮' });
const stBub = new State({ outer: 0, inner: 0 });

// ── 页内积木（卡内面板节奏；对齐 state/view 页的 label/demoCard 惯例）──
const PANE_BG = '#F8FAFC';
const PANE_BORDER = '#E2E8F0';

// 软面板：演示外围一层浅灰底盘
const pane = (w, children) => View({
    width: w, padding: 14, background: PANE_BG,
    borderColor: PANE_BORDER, borderWidth: 1, borderRadius: 10,
}, children);

// 竖分隔线
const vLine = () => View({ width: 1, height: 36, background: PANE_BORDER });

// 结果条（定宽防文案抖动）
const pill = (text, fg) => Flex({
    direction: 'row', alignItems: 'center',
    width: 300, height: 30, padding: [0, 12, 0, 12],
    borderRadius: 15, background: '#FFFFFF',
    borderColor: PANE_BORDER, borderWidth: 1,
}, [Text({ text, fontSize: 12, color: fg })]);

// 提示文字（随面板底部）
const hint = (l) => Text({
    text: t(l), fontSize: 11.5, color: '#909399',
    width: 620, wordWrap: true, margin: [12, 0, 2, 4],
});

// ── ① 四种变体 ──
const C1 = `import { Button, State, Text } from 'kwikui';

const st = new State({ msg: '点击任意变体按钮' });
Text({ text: st.msg });

// 填充 / 色调 —— 直接给背景色
Button({ text: '主要操作', background: '#2563EB', color: '#FFFFFF',
         borderRadius: 8, onClick: () => { st.msg = '点击了「主要操作」'; } });
Button({ text: '次要操作', background: '#DBEAFE', color: '#1D4ED8',
         onClick: () => { st.msg = '点击了「次要操作」'; } });

// 描边 / 幽灵 —— background 必须显式声明 'transparent'：
// 不写 background 的按钮启用主题蓝兜底（Material 语义），透明款会被盖
Button({ text: '描边操作', background: 'transparent',
         borderColor: '#2563EB', borderWidth: 1.5,
         color: '#2563EB', hoverBackground: '#EFF6FF',
         pressedBackground: '#DBEAFE',
         onClick: () => { st.msg = '点击了「描边操作」'; } });
Button({ text: '幽灵操作', background: 'transparent', color: '#2563EB',
         hoverBackground: '#EFF6FF', pressedBackground: '#DBEAFE',
         onClick: () => { st.msg = '点击了「幽灵操作」'; } });`;
const demo1 = (innerW) => [
    pane(innerW - 32, [
        Flex({ direction: 'row', gap: 14 }, [
            Flex({ direction: 'column', alignItems: 'center', gap: 8 }, [
                Button({
                    text: t(L.fBtn), width: 116, height: 34, fontSize: 13,
                    fontWeight: 'medium', background: '#2563EB', color: '#FFFFFF',
                    borderRadius: 8, shadow: '0 1 3 #94A3B840',
                    onClick: () => { stVar.msg = t(L.fMsg); },
                }),
                chip(t(L.fLbl), '#FFFFFF', '#475569'),
            ]),
            Flex({ direction: 'column', alignItems: 'center', gap: 8 }, [
                Button({
                    text: t(L.tBtn), width: 116, height: 34, fontSize: 13,
                    background: '#DBEAFE', color: '#1D4ED8', borderRadius: 8,
                    onClick: () => { stVar.msg = t(L.tMsg); },
                }),
                chip(t(L.tLbl), '#FFFFFF', '#475569'),
            ]),
            Flex({ direction: 'column', alignItems: 'center', gap: 8 }, [
                Button({
                    text: t(L.oBtn), width: 116, height: 34, fontSize: 13,
                    background: 'transparent', borderColor: '#2563EB', borderWidth: 1.5,
                    color: '#2563EB', borderRadius: 8,
                    hoverBackground: '#EFF6FF', pressedBackground: '#DBEAFE',
                    onClick: () => { stVar.msg = t(L.oMsg); },
                }),
                chip(t(L.oLbl), '#FFFFFF', '#475569'),
            ]),
            Flex({ direction: 'column', alignItems: 'center', gap: 8 }, [
                Button({
                    text: t(L.gBtn), width: 116, height: 34, fontSize: 13, color: '#2563EB',
                    background: 'transparent', borderRadius: 8,
                    hoverBackground: '#EFF6FF', pressedBackground: '#DBEAFE',
                    onClick: () => { stVar.msg = t(L.gMsg); },
                }),
                chip(t(L.gLbl), '#FFFFFF', '#475569'),
            ]),
        ]),
        pill(stVar.msg, '#2563EB'),
    ]),
    hint(L.s1Hint),
];

// ── ② 交互状态：三列微面板 ──
const C2 = `import { Button, Flex, Text } from 'kwikui';

// 只给 background：hover×0.80 / press×0.62 自动加深
Button({ text: '自动推导', background: '#2563EB' });

// 三态显式指定，覆盖推导值
Button({ text: '显式三态', background: '#0D9488',
         hoverBackground: '#0F766E', pressedBackground: '#115E59' });

// 抬升按钮：默认阴影（offsetY>0 向下投影）+ hover 全向晕开（offsetY=0）
Button({ text: '悬停阴影', background: '#FFFFFF', color: '#0F172A',
         borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8,
         shadow: '0 2 6 #94A3B840', hoverShadow: '0 0 16 #2563EB66' });

// 按下自动 0.95 缩放（内置，无需声明）`;
const demo2 = () => [
    Flex({ direction: 'row', gap: 12 }, [
        // 左：自动推导
        Flex({ direction: 'column', alignItems: 'center', gap: 8, width: 220, padding: 14,
               background: PANE_BG, borderColor: PANE_BORDER, borderWidth: 1, borderRadius: 10 }, [
            Button({
                text: t(L.actBtn), width: 128, height: 34, fontSize: 13,
                background: '#2563EB', color: '#FFFFFF', borderRadius: 8,
            }),
            Text({ text: t(L.autoLbl), fontSize: 11.5, fontWeight: 'bold', color: '#475569' }),
            Text({ text: t(L.autoCap), fontSize: 10.5, color: '#94A3B8', textAlign: 'center',
                   width: 188, wordWrap: true }),
        ]),
        // 中：显式三态
        Flex({ direction: 'column', alignItems: 'center', gap: 8, width: 220, padding: 14,
               background: PANE_BG, borderColor: PANE_BORDER, borderWidth: 1, borderRadius: 10 }, [
            Button({
                text: t(L.actBtn), width: 128, height: 34, fontSize: 13,
                background: '#0D9488', color: '#FFFFFF', borderRadius: 8,
                hoverBackground: '#0F766E', pressedBackground: '#115E59',
            }),
            Text({ text: t(L.explicitLbl), fontSize: 11.5, fontWeight: 'bold', color: '#475569' }),
            Text({ text: t(L.explicitCap), fontSize: 10.5, color: '#94A3B8', textAlign: 'center',
                   width: 188, wordWrap: true }),
        ]),
        // 右：悬停阴影
        Flex({ direction: 'column', alignItems: 'center', gap: 8, width: 220, padding: 14,
               background: PANE_BG, borderColor: PANE_BORDER, borderWidth: 1, borderRadius: 10 }, [
            Button({
                text: t(L.actBtn), width: 128, height: 34, fontSize: 13,
                background: '#FFFFFF', color: '#0F172A', borderWidth: 1,
                borderColor: '#E2E8F0', borderRadius: 8,
                shadow: '0 2 6 #94A3B840', hoverShadow: '0 0 16 #2563EB66',
            }),
            Text({ text: t(L.elevatedLbl), fontSize: 11.5, fontWeight: 'bold', color: '#475569' }),
            Text({ text: t(L.elevatedCap), fontSize: 10.5, color: '#94A3B8', textAlign: 'center',
                   width: 188, wordWrap: true }),
        ]),
    ]),
    hint(L.s2Hint),
];

// ── ③ 尺寸与形状：双组面板 ──
const C3 = `import { Button, Flex } from 'kwikui';

// 尺寸 = height + fontSize + padding 组合
Button({ text: '大按钮 L', height: 40, fontSize: 14.5, fontWeight: 'medium', padding: [22, 9] });
Button({ text: '中按钮 M', height: 34, fontSize: 13,    padding: [18, 6] });
Button({ text: '小按钮 S', height: 28, fontSize: 12,    padding: [14, 3] });

// 形状 = borderRadius；radius ≥ 半高即成胶囊（描边款：形状边缘清晰）
Button({ text: '方角', background: '#FFFFFF', borderColor: '#2563EB',
         borderWidth: 1.5, color: '#2563EB', borderRadius: 4 });
Button({ text: '常规', borderRadius: 8 });
Button({ text: '胶囊', height: 34, borderRadius: 17 });`;
const demo3 = (innerW) => [
    pane(innerW - 32, [
        Flex({ direction: 'row', alignItems: 'center', gap: 18 }, [
            Flex({ direction: 'column', gap: 10 }, [
                chip(t(L.sizeLbl), '#FFFFFF', '#475569'),
                Flex({ direction: 'row', alignItems: 'center', gap: 10 }, [
                    Button({ text: t(L.lgBtn), height: 40, fontSize: 14.5, fontWeight: 'medium',
                             background: '#2563EB', color: '#FFFFFF', borderRadius: 8,
                             padding: [22, 9] }),
                    Button({ text: t(L.mdBtn), height: 34, fontSize: 13,
                             background: '#2563EB', color: '#FFFFFF', borderRadius: 8,
                             padding: [18, 6] }),
                    Button({ text: t(L.smBtn), height: 28, fontSize: 12,
                             background: '#2563EB', color: '#FFFFFF', borderRadius: 6,
                             padding: [14, 3] }),
                ]),
            ]),
            vLine(),
            Flex({ direction: 'column', gap: 10 }, [
                chip(t(L.shapeLbl), '#FFFFFF', '#475569'),
                Flex({ direction: 'row', alignItems: 'center', gap: 10 }, [
                Button({
                    text: t(L.r4), height: 34, fontSize: 13,
                    background: '#FFFFFF', borderColor: '#2563EB', borderWidth: 1.5,
                    color: '#2563EB', borderRadius: 4, padding: [16, 5],
                }),
                Button({
                    text: t(L.r8), height: 34, fontSize: 13,
                    background: '#FFFFFF', borderColor: '#2563EB', borderWidth: 1.5,
                    color: '#2563EB', borderRadius: 8, padding: [16, 5],
                }),
                Button({
                    text: t(L.cap), height: 34, fontSize: 13,
                    background: '#FFFFFF', borderColor: '#2563EB', borderWidth: 1.5,
                    color: '#2563EB', borderRadius: 17, padding: [16, 5],
                }),
                ]),
            ]),
        ]),
    ]),
    hint(L.s3Hint),
];

// ── ④ 点击与 State 联动：左按钮列 + 右数值面板 ──
const C4 = `import { Text, Button, State } from 'kwikui';

const st = new State({ n: 0, msg: '点击任意按钮' });

Text({ text: '点击计数: ' + st.n });
Text({ text: st.msg });

Button({ text: '+1', onClick: () => { st.n = st.n + 1; } });
Button({ text: '点上取坐标', onClick: (e) => {
    // e.x/e.y = 点击点在按钮内的相对坐标（左上角原点，稳定不变量）
    // e.gx/e.gy = 窗口坐标（随滚动/摆位漂移，双载供窗口级定位）
    st.msg = '点按 @ 按钮内 (' + e.x.toFixed(0) + ', ' + e.y.toFixed(0)
           + ') 窗口 (' + e.gx.toFixed(0) + ', ' + e.gy.toFixed(0) + ')';
}});
Button({ text: '清零', onClick: () => { st.n = 0; st.msg = '计数已清零'; } });`;
const demo4 = () => [
    Flex({ direction: 'row', gap: 14 }, [
        Flex({ direction: 'column', gap: 10 }, [
            Button({
                text: t(L.incBtn), width: 84, height: 34, fontSize: 13,
                background: '#2563EB', color: '#FFFFFF', borderRadius: 8,
                onClick: () => { stEvt.n = stEvt.n + 1; },
            }),
            Button({
                text: t(L.coordBtn), width: 84, height: 34, fontSize: 12,
                background: '#DBEAFE', color: '#1D4ED8', borderRadius: 8,
                onClick: (e) => {
                    stEvt.msg = t(L.coordFmt) + ' (' + e.x.toFixed(0) + ', ' + e.y.toFixed(0)
                              + ') ' + t(L.coordWin) + ' (' + e.gx.toFixed(0) + ', ' + e.gy.toFixed(0) + ')';
                },
            }),
            Button({
                text: t(L.resetBtn), width: 84, height: 34, fontSize: 13,
                background: '#FEF2F2', color: '#B91C1C', borderRadius: 8,
                hoverBackground: '#FEE2E2',
                onClick: () => { stEvt.n = 0; stEvt.msg = t(L.resetMsg); },
            }),
        ]),
        View({
            width: 300, padding: 14, background: PANE_BG,
            borderColor: PANE_BORDER, borderWidth: 1, borderRadius: 10,
        }, [
            Text({ text: t(L.count), fontSize: 11, color: '#64748B' }),
            Text({ text: '' + stEvt.n, fontSize: 24, fontWeight: 'bold', color: '#0F172A',
                   margin: [2, 0, 0, 0] }),
            View({ width: 272, height: 1, background: PANE_BORDER, margin: [10, 0, 10, 0] }),
            Text({ text: stEvt.msg, fontSize: 12, color: '#2563EB' }),
        ]),
    ]),
    hint(L.s4Hint),
];

// ── ⑤ 点击消费与冒泡 ──
// Tap 未被消费则沿祖先链冒泡：纯文本/卡片空白 → 容器 onClick；按钮 → 消费即止
const C5 = `import { View, Text, Button, State } from 'kwikui';

const st = new State({ outer: 0, inner: 0 });

// 容器绑 onClick：只收到「未被消费」的点击
View({ padding: 16, background: '#F1F5F9', borderRadius: 8,
       onClick: () => { st.outer++; } }, [
    // 纯文本无回调 → 点击冒泡到容器，outer +1
    Text({ text: '点我试试冒泡（纯文本无回调）' }),
    // 按钮绑 onClick → 消费本层点击，容器不再收到
    Button({ text: '按钮（绑定即消费）', onClick: () => { st.inner++; } }),
]);
Text({ text: '容器收到冒泡: ' + st.outer });
Text({ text: '按钮消费计数: ' + st.inner });`;
const demo5 = () => [
    View({
        width: 560, padding: 16, background: '#F1F5F9',
        borderColor: PANE_BORDER, borderWidth: 1, borderRadius: 10,
        onClick: () => { stBub.outer = stBub.outer + 1; },
    }, [
        Text({ text: t(L.plainTxt), fontSize: 13, color: '#0F172A' }),
        Button({
            text: t(L.innerBtn), width: 170, height: 34, fontSize: 12.5,
            background: '#2563EB', color: '#FFFFFF', borderRadius: 8,
            margin: [12, 0, 0, 0],
            onClick: () => { stBub.inner = stBub.inner + 1; },
        }),
    ]),
    Flex({ direction: 'row', gap: 12, margin: [12, 0, 0, 0] }, [
        Flex({
            direction: 'row', alignItems: 'center', width: 220, height: 30,
            padding: [0, 12, 0, 12], borderRadius: 15,
            background: '#F0FDF4', borderColor: '#BBF7D0', borderWidth: 1,
        }, [Text({ text: t(L.outerCount) + ': ' + stBub.outer, fontSize: 12, color: '#16A34A' })]),
        Flex({
            direction: 'row', alignItems: 'center', width: 220, height: 30,
            padding: [0, 12, 0, 12], borderRadius: 15,
            background: '#EFF6FF', borderColor: '#BAE6FD', borderWidth: 1,
        }, [Text({ text: t(L.btnCount) + ': ' + stBub.inner, fontSize: 12, color: '#2563EB' })]),
    ]),
    hint(L.s5Hint),
];

// 页面自治：标题 + 描述 + 五个示例小节（演示紧随代码）+ 分组属性表
export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(innerW), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    ...section(t(L.s3), demo3(innerW), C3, innerW),
    ...section(t(L.s4), demo4(), C4, innerW),
    ...section(t(L.s5), demo5(), C5, innerW),
    propsTable(t, meta.props, innerW),
];
