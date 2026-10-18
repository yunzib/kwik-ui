// ── View 文档页 ──
// 页契约：meta（id / 分组 / 双语 title / desc / code / 属性表 props）
// + 内容自组装 content()（标题/描述/实况/代码/属性表的顺序与尺寸本页定）
// + demo()（实况卡内容，被 content() 调用）。演示卡、代码卡、属性表解耦，
// 公共积木来自 ui/page_parts。
import { View, Flex, Text, State } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc, pageCode } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';

export const meta = {
    id: 'view',
    group: 'base',
    title: { zh: 'View 视图', en: 'View' },
    desc: {
        zh: '最基础的容器组件：背景 / 边框 / 圆角 / 阴影 / 内外边距 / 透明度，'
            + '并可挂 onClick 点击回调；所有组件共承这套通用属性。',
        en: 'The base container: background / border / radius / shadow / padding '
            + '/ margin / opacity, plus onClick; every component inherits this set.',
    },
    code: `import { View, Flex, Text, State } from 'kwikui';

const st = new State({ i: 0 });
const COLORS = ['#2563EB', '#16A34A', '#E5484D'];

export const demo = () => Flex({ direction: 'row' }, [
    // 纯样式：背景 + 圆角 + 阴影
    View({ width: 120, height: 72, background: '#2563EB',
           borderRadius: 10, shadow: '0 6 18 #2563EB55' }),
    // 边框 + 内边距（子内容被 padding 推入）
    View({ width: 150, height: 72, borderColor: '#CBD5E1',
           borderWidth: 1, borderRadius: 8, padding: [10, 0, 0, 12] }, [
        Text({ text: '内容', fontSize: 13 }),
    ]),
    // 交互：点击换背景色（State 写 → reconcile）
    View({ width: 120, height: 72, borderRadius: 10,
           background: COLORS[st.i],
           onClick: () => { st.i = (st.i + 1) % COLORS.length; } }, [
        Text({ text: '点我', fontSize: 13, color: '#FFFFFF' }),
    ]),
]);`,
    // 属性表：[name, type, default, {zh,en}]
    props: [
        ['id', 'string', '-', { zh: '组件标识（getProp/setProp 按 id 定位）', en: 'Identifier for getProp/setProp lookup' }],
        ['width / height', 'number', '-', { zh: '固定宽高（px）；不设则随内容/父约束自适应', en: 'Fixed size in px; unset = content/parent-driven' }],
        ['background', 'string', 'transparent', { zh: '背景色（#RRGGBB / #RRGGBBAA）', en: 'Background color' }],
        ['borderRadius', 'number', '0', { zh: '圆角半径（px）', en: 'Corner radius in px' }],
        ['borderWidth', 'number', '0', { zh: '边框粗细（px）', en: 'Border width in px' }],
        ['borderColor', 'string', '-', { zh: '边框颜色', en: 'Border color' }],
        ['padding', 'number | [h,v] | [l,t,r,b]', '0', { zh: '内边距：四值数组按左/上/右/下', en: 'Inner spacing; 4-tuple = left/top/right/bottom' }],
        ['margin', 'number | [h,v] | [l,t,r,b]', '0', { zh: '外边距（布局容器消费）', en: 'Outer spacing (consumed by layout parents)' }],
        ['shadow', 'string', '-', { zh: "盒阴影 'offsetX offsetY blur color'", en: "Box shadow 'offsetX offsetY blur color'" }],
        ['opacity', 'number', '1', { zh: '不透明度 0..1', en: 'Opacity 0..1' }],
        ['visible', 'boolean', 'true', { zh: '是否可见（false 摘除渲染）', en: 'Visibility (false removes from composition)' }],
        ['onClick', '(e) => boolean', '-', { zh: '点击回调；返回 true 消费并阻止冒泡', en: 'Tap callback; return true to consume & stop bubbling' }],
    ],
};

const L = {
    style: { zh: '样式', en: 'Style' },
    bordered: { zh: '内容', en: 'Content' },
    tapMe: { zh: '点我换色', en: 'Tap me' },
    hint: {
        zh: '三个方块：圆角+阴影 / 边框+内边距 / 点击换色（State 响应）。',
        en: 'Three tiles: radius+shadow / border+padding / tap to recolor (reactive State).',
    },
};

const COLORS = ['#2563EB', '#16A34A', '#E5484D'];

// State 必须模块顶层：demo() 每次重建都会重跑（State 写 → 整树 reconcile），
// 函数内 new State 每轮都是全新对象，换色永远停在首色
const st = new State({ i: 0 });

// 实况卡随内容区净宽伸缩（main 下传 innerW；默认 884 保持本文件可独立运行）
export const demo = (innerW = 884) => {
    return View({
        width: innerW, padding: 16, background: '#FFFFFF',
        borderColor: '#E2E8F0', borderWidth: 1, borderRadius: 8,
        margin: [0, 0, 24, 4],
    }, [
        Flex({ direction: 'row' }, [
            // 纯样式：背景 + 圆角 + 阴影
            View({
                width: 140, height: 72, background: '#2563EB',
                borderRadius: 10, shadow: '0 6 18 #2563EB55', margin: [0, 14, 8, 0],
            }, [
                Text({ text: t(L.style), fontSize: 13, color: '#FFFFFF', padding: [8, 0, 0, 12] }),
            ]),
            // 边框 + 内边距（子内容被 padding 推入）
            View({
                width: 140, height: 72, borderColor: '#CBD5E1',
                borderWidth: 1, borderRadius: 8, padding: [10, 0, 0, 12], margin: [0, 14, 8, 0],
            }, [
                Text({ text: t(L.bordered), fontSize: 13, color: '#334155' }),
            ]),
            // 交互：点击换背景色（State 写 → reconcile）
            View({
                width: 140, height: 72, borderRadius: 10,
                background: COLORS[st.i], margin: [0, 14, 8, 0],
                onClick: () => { st.i = (st.i + 1) % COLORS.length; },
            }, [
                Text({ text: t(L.tapMe), fontSize: 13, color: '#FFFFFF', padding: [8, 0, 0, 12] }),
            ]),
        ]),
        Text({ text: t(L.hint), fontSize: 12, color: '#909399', margin: [4, 0, 0, 0] }),
    ]);
};

// 页面自治：标题 + 描述 + 实况卡 + 代码卡 + 属性表（meta.props 在声明页渲染，
// 未声明页不挂——挂哪、位置如何都是本页的事）
export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    demo(innerW),
    pageCode(meta, 320, innerW),
    ...(meta.props ? [propsTable(t, meta.props, innerW)] : []),
];
