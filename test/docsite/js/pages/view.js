// ── View 文档页 ──
// 页契约：meta（id / 分组 / 双语 title / desc / 分组属性表 props）
// + 内容自组装 content()。本页结构 = 标题/描述 + 若干示例小节
// （每节：小节标题 → 实况演示卡 → 紧随其下的示例代码卡）+ 分组属性表。
// 公共积木来自 ui/page_parts（pageTitle/pageDesc）与 ui/code_block。
import { View, Flex, Text, State } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { codeBlock } from '../ui/code_block.js';
import { propsTable } from '../ui/props_table.js';

export const meta = {
    id: 'view',
    group: 'base',
    title: { zh: 'View 视图', en: 'View' },
    desc: {
        zh: '最基础的容器组件：尺寸 / 背景 / 边框 / 圆角 / 阴影 / 内外边距 / '
            + '透明度 / 百分比尺寸，并可挂 onClick 点击回调；所有组件共承这套通用属性。',
        en: 'The base container: size / background / border / radius / shadow / padding '
            + '/ margin / opacity / percentage sizing, plus onClick; every component inherits this set.',
    },
    // 属性表（分组结构）：[{ title: {zh,en}, rows: [[name, type, default, {zh,en}], ...] }]
    props: [
        { title: { zh: '基础', en: 'Basics' }, rows: [
            ['id', 'string', '-', { zh: '组件标识（getProp/setProp 按 id 定位）', en: 'Identifier for getProp/setProp lookup' }],
            ['width / height', 'number', '-', { zh: '固定宽高（px）；不设则随内容/父约束自适应', en: 'Fixed size in px; unset = content/parent-driven' }],
            ['widthPct / heightPct', 'number (0..1)', '-', { zh: '百分比尺寸：width 传 "50%" 自动解析为 0.5（基准 = 父容器内容尺寸）', en: 'Percentage size: "50%" parses to 0.5 (relative to parent content box)' }],
        ]},
        { title: { zh: '布局与定位', en: 'Layout & Position' }, rows: [
            ['margin', 'number | [h,v] | [t,r,b,l]', '0', { zh: '外边距（布局容器消费）；四值数组按上/右/下/左（CSS 顺时针）', en: 'Outer spacing (consumed by layout parents); 4-tuple = top/right/bottom/left (CSS order)' }],
            ['padding', 'number | [h,v] | [t,r,b,l]', '0', { zh: '内边距：内容被推入；四值数组按上/右/下/左（CSS 顺时针）', en: 'Inner spacing; 4-tuple = top/right/bottom/left (CSS order)' }],
            ['align', 'string', 'default', { zh: '父容器交叉轴对齐覆盖（start/center/end/stretch）', en: 'Cross-axis alignment override in layout parent' }],
            ['flexGrow / flexShrink', 'number', '0', { zh: '弹性生长/收缩比（Flex/列表布局消费）', en: 'Flex grow/shrink factor (consumed by flex layouts)' }],
            ['absolute', 'boolean', 'false', { zh: '绝对定位（Stack 布局消费；配合 absTop/absLeft/absRight/absBottom）', en: 'Absolute positioning in Stack layout (with absTop/absLeft/…)' }],
            ['z', 'number', '0', { zh: '层叠优先级：值越高越优先命中/绘制', en: 'Stacking priority; higher = on top & hit first' }],
        ]},
        { title: { zh: '外观', en: 'Appearance' }, rows: [
            ['background', 'string', 'transparent', { zh: '背景色（#RRGGBB / #RRGGBBAA）', en: 'Background color (#RRGGBB / #RRGGBBAA)' }],
            ['gradient', 'string', '-', { zh: '背景渐变（存在时优先于 background 填充）', en: 'Background gradient; takes precedence over background' }],
            ['opacity', 'number (0..1)', '1', { zh: '不透明度', en: 'Opacity' }],
            ['visible', 'boolean', 'true', { zh: '是否可见（false 摘除渲染与命中）', en: 'Visibility (false removes from composition & hit test)' }],
            ['backdropBlur', 'number', '0', { zh: '液态玻璃：就地模糊其下层内容作底（px，0=关闭）', en: 'Glass effect: blur content behind (px; 0 = off)' }],
        ]},
        { title: { zh: '边框与圆角', en: 'Border & Radius' }, rows: [
            ['borderRadius', 'number', '0', { zh: '圆角半径（px）', en: 'Corner radius in px' }],
            ['borderWidth', 'number', '0', { zh: '边框粗细（px）', en: 'Border width in px' }],
            ['borderColor', 'string', '-', { zh: '边框颜色', en: 'Border color' }],
            ['borderStyle', 'string', 'solid', { zh: '边框线型（solid 实线）', en: 'Border line style (solid)' }],
        ]},
        { title: { zh: '阴影', en: 'Shadow' }, rows: [
            ['shadow', 'string', '-', { zh: "盒阴影 'offsetX offsetY blur color'（如 '0 8 20 #2563EB88'）", en: "Box shadow 'offsetX offsetY blur color'" }],
        ]},
        { title: { zh: '交互', en: 'Interaction' }, rows: [
            ['onClick', '(e) => boolean', '-', { zh: '点击回调；返回 true 消费并阻止冒泡', en: 'Tap callback; return true to consume & stop bubbling' }],
        ]},
    ],
};

// ── 双语词典（小节标题与演示标签）──
const L = {
    s1: { zh: '背景与圆角', en: 'Background & Radius' },
    s2: { zh: '边框', en: 'Border' },
    s3: { zh: '内边距与外边距', en: 'Padding & Margin' },
    s4: { zh: '阴影', en: 'Shadow' },
    s5: { zh: '透明度', en: 'Opacity' },
    s6: { zh: '百分比尺寸', en: 'Percentage Size' },
    s7: { zh: '点击交互', en: 'Click Interaction' },
    r0: { zh: '圆角 0', en: 'radius 0' },
    r10: { zh: '圆角 10', en: 'radius 10' },
    r20: { zh: '圆角 20', en: 'radius 20' },
    r32: { zh: '圆角 32', en: 'radius 32' },
    thin: { zh: '细边框', en: 'thin' },
    medium: { zh: '中边框', en: 'medium' },
    thick: { zh: '粗边框', en: 'thick' },
    padLabel: { zh: '内容被 padding 推入', en: 'pushed in by padding' },
    marginHint: { zh: '两块间距 24 = 右块 margin 左值', en: 'gap 24 = left margin of right block' },
    o3: { zh: '0.3', en: '0.3' },
    o6: { zh: '0.6', en: '0.6' },
    o1: { zh: '1.0', en: '1.0' },
    pct50: { zh: '50% 宽', en: '50% width' },
    pct25: { zh: '25% 宽', en: '25% width' },
    tapMe: { zh: '点我换色', en: 'Tap me' },
    tapHint: {
        zh: '点击写 State → 整树 reconcile → 背景随扩展翻转。',
        en: 'Tap writes State → tree reconciles → background flips.',
    },
};

const COLORS = ['#2563EB', '#16A34A', '#E5484D'];

// State 必须模块顶层：content() 每次重建都会重跑（State 写 → 整树 reconcile），
// 函数内 new State 每轮都是全新对象，换色永远停在首色
const st = new State({ i: 0 });

// 演示块内标签（小字白/深灰，绝对定位风格用 padding 推入）
const label = (txt, dark) => Text({
    text: txt, fontSize: 11.5, color: dark ? '#334155' : '#FFFFFF',
    padding: [4, 0, 0, 8],
});

// 演示卡：白底圆角边框卡，包住当节演示内容
const demoCard = (children, innerW) => View({
    width: innerW, padding: 16, background: '#FFFFFF',
    borderColor: '#E2E8F0', borderWidth: 1, borderRadius: 8,
    margin: [0, 0, 8, 4],
}, children);

// 代码高度按行数自适应（行高 19 + 上下 padding 24 + 余量）
const codeH = (code) => code.split('\n').length * 19 + 26;

// 示例小节：小节标题 → 演示卡 → 紧随其下的示例代码卡
const section = (title, children, code, innerW) => [
    Text({ text: title, fontSize: 15, fontWeight: 'bold', color: '#0F172A', margin: [14, 0, 8, 4] }),
    demoCard(children, innerW),
    codeBlock(t, code, codeH(code), innerW),
];

// ── ① 背景与圆角 ──
const C1 = `import { View, Flex } from 'kwikui';

// 背景纯色 + 圆角梯度（borderRadius: px）
Flex({ direction: 'row', gap: 14 }, [
    View({ width: 96, height: 64, background: '#2563EB', borderRadius: 0 }),
    View({ width: 96, height: 64, background: '#2563EB', borderRadius: 10 }),
    View({ width: 96, height: 64, background: '#2563EB', borderRadius: 20 }),
    View({ width: 96, height: 64, background: '#2563EB', borderRadius: 32 }),
]);`;
const demo1 = () => [Flex({ direction: 'row', gap: 14 }, [
    View({ width: 96, height: 64, background: '#2563EB', borderRadius: 0 }, [label(t(L.r0))]),
    View({ width: 96, height: 64, background: '#2563EB', borderRadius: 10 }, [label(t(L.r10))]),
    View({ width: 96, height: 64, background: '#2563EB', borderRadius: 20 }, [label(t(L.r20))]),
    View({ width: 96, height: 64, background: '#2563EB', borderRadius: 32 }, [label(t(L.r32))]),
])];

// ── ② 边框 ──
const C2 = `import { View, Flex } from 'kwikui';

// borderWidth 粗细 + borderColor 颜色（可与圆角叠加）
Flex({ direction: 'row', gap: 14 }, [
    View({ width: 110, height: 64, borderColor: '#94A3B8', borderWidth: 1, borderRadius: 8 }),
    View({ width: 110, height: 64, borderColor: '#2563EB', borderWidth: 2, borderRadius: 10 }),
    View({ width: 110, height: 64, borderColor: '#E5484D', borderWidth: 4, borderRadius: 14 }),
]);`;
const demo2 = () => [Flex({ direction: 'row', gap: 14 }, [
    View({ width: 110, height: 64, borderColor: '#94A3B8', borderWidth: 1, borderRadius: 8 }, [label(t(L.thin), true)]),
    View({ width: 110, height: 64, borderColor: '#2563EB', borderWidth: 2, borderRadius: 10 }, [label(t(L.medium), true)]),
    View({ width: 110, height: 64, borderColor: '#E5484D', borderWidth: 4, borderRadius: 14 }, [label(t(L.thick), true)]),
])];

// ── ③ 内边距与外边距 ──
// margin 四值数组按 上/右/下/左（CSS 顺时针）：右块的左外边距 = 与左块的间距
const C3 = `import { View, Flex, Text } from 'kwikui';

// padding：内容被推入（[上, 右, 下, 左] —— 此处上 14 左 14）
View({ width: 170, height: 64, background: '#EFF6FF',
       borderColor: '#2563EB', borderWidth: 1,
       padding: [14, 0, 0, 14] }, [
    Text({ text: '内容', fontSize: 13 }),
]);
// margin：块间间距 = 右块的左外边距（布局容器消费）
View({ width: 120, height: 40, background: '#16A34A' });
View({ width: 120, height: 40, background: '#E5484D', margin: [0, 0, 0, 24] });`;
const demo3 = () => [Flex({ direction: 'row', alignItems: 'center' }, [
    View({
        width: 170, height: 64, background: '#EFF6FF',
        borderColor: '#2563EB', borderWidth: 1,
        padding: [14, 0, 0, 14],
    }, [Text({ text: t(L.padLabel), fontSize: 12, color: '#334155' })]),
    View({ width: 120, height: 40, background: '#16A34A', borderRadius: 6 }),
    View({ width: 120, height: 40, background: '#E5484D', borderRadius: 6, margin: [0, 0, 0, 24] }),
    Text({ text: t(L.marginHint), fontSize: 11.5, color: '#909399', margin: [0, 0, 0, 16] }),
])];

// ── ④ 阴影 ──
// 演示块浅灰底 + 细边框：块本体在白底卡上恒可见，阴影效果围绕块缘对比明确
const C4 = `import { View, Flex } from 'kwikui';

// shadow: 'offsetX offsetY blur color'——偏移/模糊递进
Flex({ direction: 'row', gap: 24 }, [
    View({ width: 110, height: 64, background: '#F1F5F9',
           borderColor: '#CBD5E1', borderWidth: 1, borderRadius: 10,
           shadow: '0 2 8 #94A3B888' }),
    View({ width: 110, height: 64, background: '#F1F5F9',
           borderColor: '#CBD5E1', borderWidth: 1, borderRadius: 10,
           shadow: '0 8 20 #2563EB88' }),
    View({ width: 110, height: 64, background: '#F1F5F9',
           borderColor: '#CBD5E1', borderWidth: 1, borderRadius: 10,
           shadow: '0 14 30 #E5484D99' }),
]);`;
const demo4 = () => [Flex({ direction: 'row', gap: 24 }, [
    View({ width: 110, height: 64, background: '#F1F5F9', borderColor: '#CBD5E1', borderWidth: 1, borderRadius: 10, shadow: '0 2 8 #94A3B888' }),
    View({ width: 110, height: 64, background: '#F1F5F9', borderColor: '#CBD5E1', borderWidth: 1, borderRadius: 10, shadow: '0 8 20 #2563EB88' }),
    View({ width: 110, height: 64, background: '#F1F5F9', borderColor: '#CBD5E1', borderWidth: 1, borderRadius: 10, shadow: '0 14 30 #E5484D99' }),
])];

// ── ⑤ 透明度 ──
const C5 = `import { View, Flex } from 'kwikui';

// opacity: 0..1（0.3 / 0.6 / 1.0 同色递进）
Flex({ direction: 'row', gap: 14 }, [
    View({ width: 96, height: 64, background: '#2563EB', borderRadius: 10, opacity: 0.3 }),
    View({ width: 96, height: 64, background: '#2563EB', borderRadius: 10, opacity: 0.6 }),
    View({ width: 96, height: 64, background: '#2563EB', borderRadius: 10, opacity: 1.0 }),
]);`;
const demo5 = () => [Flex({ direction: 'row', gap: 14 }, [
    View({ width: 96, height: 64, background: '#2563EB', borderRadius: 10, opacity: 0.3 }, [label(t(L.o3))]),
    View({ width: 96, height: 64, background: '#2563EB', borderRadius: 10, opacity: 0.6 }, [label(t(L.o6))]),
    View({ width: 96, height: 64, background: '#2563EB', borderRadius: 10, opacity: 1.0 }, [label(t(L.o1))]),
])];

// ── ⑥ 百分比尺寸 ──
const C6 = `import { View, Text } from 'kwikui';

// width 传百分比串：相对父容器内容宽自动解析（"50%" → 0.5）
View({ width: '50%', height: 40, background: '#2563EB', borderRadius: 6 });
View({ width: '25%', height: 40, background: '#16A34A', borderRadius: 6,
       margin: [8, 0, 0, 0] });`;
const demo6 = () => [
    View({ width: '50%', height: 40, background: '#2563EB', borderRadius: 6 }, [label(t(L.pct50))]),
    View({ width: '25%', height: 40, background: '#16A34A', borderRadius: 6, margin: [8, 0, 0, 0] }, [label(t(L.pct25))]),
];

// ── ⑦ 点击交互 ──
const C7 = `import { View, Text, State } from 'kwikui';

const st = new State({ i: 0 });
const COLORS = ['#2563EB', '#16A34A', '#E5484D'];

// 点击写 State → 整树 reconcile → 背景随扩展翻转
View({
    width: 150, height: 64, borderRadius: 10,
    background: COLORS[st.i],
    onClick: () => { st.i = (st.i + 1) % COLORS.length; },
}, [
    Text({ text: '点我换色', fontSize: 13, color: '#FFFFFF' }),
]);`;
const demo7 = () => [
    Flex({ direction: 'row', alignItems: 'center' }, [
        View({
            width: 150, height: 64, borderRadius: 10,
            background: COLORS[st.i],
            onClick: () => { st.i = (st.i + 1) % COLORS.length; },
        }, [label(t(L.tapMe))]),
        Text({ text: t(L.tapHint), fontSize: 11.5, color: '#909399', margin: [0, 0, 0, 16] }),
    ]),
];

// 页面自治：标题 + 描述 + 七个示例小节（每节演示紧随代码）+ 分组属性表
export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    ...section(t(L.s3), demo3(), C3, innerW),
    ...section(t(L.s4), demo4(), C4, innerW),
    ...section(t(L.s5), demo5(), C5, innerW),
    ...section(t(L.s6), demo6(), C6, innerW),
    ...section(t(L.s7), demo7(), C7, innerW),
    propsTable(t, meta.props, innerW),
];
