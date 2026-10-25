// ── Layer 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/layer.md。
// 单演示小节：模态浮层（中心定位 + 遮罩）。
import { View, Flex, Text, Button, Layer, State, ref } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'layer',
    group: 'layout',
    title: { zh: 'Layer 浮层', en: 'Layer' },
    desc: {
        zh: '统一浮层原语：容器模式按视口锚点或目标锚定摆放，自由模式则全屏让子项用 x / y 定位。'
            + '浮层走 LayerStack，始终绘制在 View 树最上层。',
        en: 'Unified overlay primitive: container mode anchors to the viewport or a target element; '
            + 'free mode is a full-screen canvas with x / y children. Layers draw above the whole tree.',
    },
    props: [
        { title: { zh: '显隐与模式', en: 'Visibility & Mode' }, rows: [
            ['active（别名 open）', 'boolean | ref', 'false', { zh: '是否显示', en: 'Visible' }],
            ['modal', 'boolean', 'false', { zh: '遮罩 + 阻断背景（false=穿透）', en: 'Mask + block (false = pass-through)' }],
            ['maskColor / maskClosable', 'color / boolean', 'rgba(0,0,0,0.4) / true', { zh: '遮罩色 / 点遮罩关闭', en: 'Mask color / tap-to-close' }],
            ['transparent', 'boolean', 'false', { zh: '无遮罩无容器全穿透（tooltip/toast）', en: 'No mask/container, full pass-through' }],
        ] },
        { title: { zh: '定位与容器', en: 'Position & Container' }, rows: [
            ['anchor', 'string', "''", { zh: '目标元素 id（空=视口模式）', en: 'Target element id (empty = viewport)' }],
            ['position', '9 锚点 | out-top/out-bottom/out-left/out-right', 'center', { zh: '视口或锚定方位', en: 'Viewport or anchored position' }],
            ['offsetX / offsetY', 'number', '0', { zh: '位置偏移', en: 'Position offset' }],
            ['width / height', 'number', '0', { zh: '容器尺寸（0=自适应）', en: 'Container size (0 = auto)' }],
            ['background / borderRadius / padding', 'color / number / edge', '#FFFFFF / 8 / 24', { zh: '容器外观', en: 'Container styling' }],
            ['onClose', '() => {}', '-', { zh: '遮罩 / ESC / active=false 关闭时触发', en: 'Fired on mask / ESC / close' }],
        ] },
    ],
};

const L = {
    s1: { zh: '模态浮层', en: 'Modal layer' },
    openLayer: { zh: '打开模态浮层', en: 'Open modal layer' },
};

const st = new State({ open: false });

const C1 = `const ui = new State({ open: false });

Layer({
    active: ref(ui, 'open'),
    modal: true, maskClosable: true,
    position: 'center', width: 340,
    onClose: () => { ui.open = false; },
}, [
    Text({ text: '确认操作' }),
    Button({ text: '关闭', onClick: () => { ui.open = false; } }),
]);`;
const demo1 = () => [
    Button({
        text: t(L.openLayer), width: 150, height: 34, fontSize: 12.5,
        background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
        onClick: () => { st.open = true; },
    }),
    Text({ text: '模态浮层打开后背景被遮罩阻断，点遮罩或关闭按钮退出。', fontSize: 11.5, color: '#94A3B8', margin: [0, 8, 0, 0] }),
    Layer({
        id: 'ds-layer', active: ref(st, 'open'), modal: true,
        maskClosable: true, position: 'center', width: 340,
        background: '#FFFFFF', borderRadius: 12, padding: 20,
        onClose: () => { st.open = false; },
    }, [
        Text({ text: '确认操作', fontSize: 17, fontWeight: 'bold', color: '#0F172A' }),
        Text({ text: '浮层内容位于独立图层，不会被祖先裁剪。', fontSize: 12.5, color: '#64748B', margin: [8, 0, 18, 0] }),
        Flex({ direction: 'row', justifyContent: 'end', gap: 8 }, [
            Button({ text: '关闭', width: 76, height: 30, fontSize: 12,
                     background: '#EEF2F7', color: '#334155', borderRadius: 6,
                     onClick: () => { st.open = false; } }),
        ]),
    ]),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
