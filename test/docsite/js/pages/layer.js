// ── Layer 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/layer.md。
// 四个示例小节：模态浮层 / 锚定 tooltip / 视口通知 / 自由模式菜单。
// 示例代码与演示同构：C1–C4 逐行镜像 demo1–demo4（改演示必须同步改代码，
// 反之亦然；同 flex / grid / stack / list / scrollview / lazylist / stackindex 纪律）。
import { View, Flex, Text, Button, Layer, State, ref, setProp, setTimeout } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'layer',
    group: 'layout',
    title: { zh: 'Layer 浮层', en: 'Layer' },
    desc: {
        zh: '统一浮层原语：容器模式（width / height / anchor 任一非空）按视口 9 锚点或目标锚定摆放，'
            + '自由模式全屏、子项用 x / y 定位；浮层走 LayerStack 始终绘制在 View 树最上层、'
            + '不被祖先裁剪。modal 遮罩阻断，transparent 全穿透（tooltip / toast）；'
            + 'active 经 ref 绑定或 setProp 命令式开关，关闭触发 onClose。',
        en: 'Unified overlay primitive: container mode (any of width / height / anchor set) anchors '
            + 'to the viewport (9 positions) or a target element; free mode is a fullscreen canvas '
            + 'with x / y children. Layers ride the LayerStack above the whole tree, never clipped '
            + 'by ancestors. modal masks and blocks, transparent passes everything through '
            + '(tooltips / toasts); active toggles via ref binding or setProp, closing fires onClose.',
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
            ['position', '9 锚点 | out-top/bottom/left/right', "'center'", { zh: '视口或锚定方位', en: 'Viewport or anchored position' }],
            ['offsetX / offsetY', 'number', '0', { zh: '位置偏移', en: 'Position offset' }],
            ['width / height', 'number', '0', { zh: '容器尺寸（0=自适应）', en: 'Container size (0 = auto)' }],
            ['background / borderRadius / padding', 'color / number / edge', "'#FFFFFF' / 8 / 24", { zh: '容器外观', en: 'Container styling' }],
            ['onClose', '() => {}', '-', { zh: '遮罩 / ESC / active=false 关闭时触发', en: 'Fired on mask / ESC / close' }],
        ] },
        { title: { zh: '命令式', en: 'Imperative' }, rows: [
            ['setProp(id, "active" / "open", v)', '-', '-', { zh: '显式开关（本页 tooltip / 通知 / 菜单均走此通路）', en: 'Explicit toggle (used by the tooltip / toast / menu demos)' }],
        ] },
    ],
};

const L = {
    s1: { zh: '模态浮层', en: 'Modal layer' },
    s2: { zh: '锚定 tooltip', en: 'Anchored tooltip' },
    s3: { zh: '视口通知（9 锚点）', en: 'Viewport toast' },
    s4: { zh: '自由模式菜单', en: 'Free-mode menu' },
    openLayer: { zh: '打开模态浮层', en: 'Open modal layer' },
    close: { zh: '关闭', en: 'Close' },
    confirmTitle: { zh: '确认操作', en: 'Confirm' },
    confirmBody: { zh: '浮层内容位于独立图层，不会被祖先裁剪。', en: 'Layer content rides an independent stack, never clipped by ancestors.' },
    hoverTip: { zh: '悬停看提示', en: 'Hover me' },
    tipText: { zh: '锚定在目标上方的提示', en: 'Anchored above the target' },
    notify: { zh: '弹出右下通知', en: 'Pop bottom-right toast' },
    ok: { zh: '操作成功', en: 'Success' },
    saved: { zh: '文件已保存到本地（2.4 秒后自动关闭）', en: 'File saved locally (auto-closes in 2.4s)' },
    menu: { zh: '打开自由菜单', en: 'Open free menu' },
    miNew: { zh: '新建项目', en: 'New project' },
    miOpen: { zh: '打开文件', en: 'Open file' },
    miQuit: { zh: '退出', en: 'Quit' },
    hint1: { zh: 'modal=true 遮罩 + 阻断背景（maskClosable 点遮罩关闭、ESC 也可关）；'
            + 'active 用 ref 双向绑定，onClose 在遮罩 / ESC / active=false 各关闭路径都会触发。',
             en: 'modal=true masks and blocks the background (maskClosable closes on mask clicks, '
             + 'ESC works too); active is ref-bound two-way and onClose fires on every close path '
             + '(mask / ESC / active=false)' },
    hint2: { zh: 'anchor 指向目标元素 id + position:"out-top"（另有 out-bottom / out-left / '
            + 'out-right / center）；transparent=true 全穿透不挡背景；开关走 setProp——'
            + '目标按钮 onHoverEnter / onHoverLeave 触发。',
             en: 'anchor targets an element id with position:"out-top" (also out-bottom / '
             + 'out-left / out-right / center); transparent=true passes everything through; the '
             + 'toggle goes through setProp driven by the target\'s onHoverEnter / onHoverLeave' },
    hint3: { zh: 'anchor 为空即视口模式：position 取 9 锚点（本例 bottomRight）+ offsetX / offsetY '
            + '微调；非模态无遮罩不挡背景，setTimeout 到点 setProp 关闭。',
             en: 'An empty anchor selects viewport mode: position picks one of the nine anchors '
             + '(bottomRight here) with offsetX / offsetY trims; non-modal means no mask and no '
             + 'blocking, and a setTimeout closes it via setProp' },
    hint4: { zh: 'width / height / anchor 全空 → 自由模式：全屏层、children 用 x / y 自由定位'
            + '（toast / 自绘浮层）；transparent 穿透层外点击，菜单项点击后 setProp 关闭。',
             en: 'With width / height / anchor all empty the layer goes free-mode: a fullscreen '
             + 'canvas whose children place by x / y (toasts / custom overlays); transparent lets '
             + 'outside clicks pass and menu items close via setProp' },
};

const st = new State({ open: false });

const C1 = `import { Flex, Text, Button, Layer, State, ref } from 'kwikui';

const ui = new State({ open: false });

Button({ text: '打开模态浮层', width: 150, height: 34, fontSize: 12.5,
         background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
         onClick: () => { ui.open = true; } });

Layer({
    active: ref(ui, 'open'),
    modal: true, maskClosable: true,
    position: 'center', width: 340,
    background: '#FFFFFF', borderRadius: 12, padding: 20,
    onClose: () => { ui.open = false; },
}, [
    Text({ text: '确认操作', fontSize: 17, fontWeight: 'bold', color: '#0F172A' }),
    Text({ text: '浮层内容位于独立图层，不会被祖先裁剪。', fontSize: 12.5, color: '#64748B',
           margin: [8, 0, 18, 0] }),
    Flex({ direction: 'row', justifyContent: 'end', gap: 8 }, [
        Button({ text: '关闭', width: 76, height: 30, fontSize: 12,
                 background: '#EEF2F7', color: '#334155', borderRadius: 6,
                 onClick: () => { ui.open = false; } }),
    ]),
]);`;

const demo1 = () => [
    Flex({ direction: 'row', alignItems: 'center', gap: 10 }, [
        Button({
            text: t(L.openLayer), width: 150, height: 34, fontSize: 12.5,
            background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
            onClick: () => { st.open = true; },
        }),
    ]),
    Layer({
        active: ref(st, 'open'), modal: true,
        maskClosable: true, position: 'center', width: 340,
        background: '#FFFFFF', borderRadius: 12, padding: 20,
        onClose: () => { st.open = false; },
    }, [
        Text({ text: t(L.confirmTitle), fontSize: 17, fontWeight: 'bold', color: '#0F172A' }),
        Text({ text: t(L.confirmBody), fontSize: 12.5, color: '#64748B', margin: [8, 0, 18, 0] }),
        Flex({ direction: 'row', justifyContent: 'end', gap: 8 }, [
            Button({ text: t(L.close), width: 76, height: 30, fontSize: 12,
                     background: '#EEF2F7', color: '#334155', borderRadius: 6,
                     onClick: () => { st.open = false; } }),
        ]),
    ]),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C2 = `import { Text, Button, Layer, setProp } from 'kwikui';

// 目标按钮：悬停开 / 移出关（命令式开关）
Button({ id: 'tip-anchor', text: '悬停看提示', width: 130, height: 32, fontSize: 12,
         background: '#EEF2F7', color: '#334155', borderRadius: 6,
         onHoverEnter: () => setProp('tt-layer', 'active', 'true'),
         onHoverLeave: () => setProp('tt-layer', 'active', 'false') });

Layer({
    id: 'tt-layer', active: false, transparent: true,
    anchor: 'tip-anchor', position: 'out-top', offsetY: 6,
    background: 'rgba(60,60,67,0.9)', borderRadius: 4, padding: [4, 8],
}, [Text({ text: '锚定在目标上方的提示', fontSize: 12, color: '#FFFFFF' })]);`;

const demo2 = () => [
    Button({
        id: 'tip-anchor', text: t(L.hoverTip), width: 130, height: 32, fontSize: 12,
        background: '#EEF2F7', color: '#334155', borderRadius: 6,
        onHoverEnter: () => setProp('tt-layer', 'active', 'true'),
        onHoverLeave: () => setProp('tt-layer', 'active', 'false'),
    }),
    Layer({
        id: 'tt-layer', active: false, transparent: true,
        anchor: 'tip-anchor', position: 'out-top', offsetY: 6,
        background: 'rgba(60,60,67,0.9)', borderRadius: 4, padding: [4, 8],
    }, [Text({ text: t(L.tipText), fontSize: 12, color: '#FFFFFF' })]),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C3 = `import { Text, Button, Layer, setProp } from 'kwikui';

// 视口 9 锚点：bottomRight + 偏移；setTimeout 到点自动关闭
Button({ text: '弹出右下通知', width: 140, height: 32, fontSize: 12,
         background: '#EEF2F7', color: '#334155', borderRadius: 6,
         onClick: () => {
             setProp('note-layer', 'active', 'true');
             setTimeout(() => setProp('note-layer', 'active', 'false'), 2400);
         } });

Layer({
    id: 'note-layer', active: false, modal: false,
    position: 'bottomRight', offsetX: 20, offsetY: 20,
    width: 280, background: '#FFFFFF', borderRadius: 12, padding: 16,
}, [
    Text({ text: '操作成功', fontSize: 15, fontWeight: 'bold', color: '#2E7D32' }),
    Text({ text: '文件已保存到本地（2.4 秒后自动关闭）', fontSize: 12.5, color: '#64748B',
           margin: [6, 0, 0, 0] }),
]);`;

const demo3 = () => [
    Button({
        text: t(L.notify), width: 140, height: 32, fontSize: 12,
        background: '#EEF2F7', color: '#334155', borderRadius: 6,
        onClick: () => {
            setProp('note-layer', 'active', 'true');
            setTimeout(() => setProp('note-layer', 'active', 'false'), 2400);
        },
    }),
    Layer({
        id: 'note-layer', active: false, modal: false,
        position: 'bottomRight', offsetX: 20, offsetY: 20,
        width: 280, background: '#FFFFFF', borderRadius: 12, padding: 16,
    }, [
        Text({ text: t(L.ok), fontSize: 15, fontWeight: 'bold', color: '#2E7D32' }),
        Text({ text: t(L.saved), fontSize: 12.5, color: '#64748B', margin: [6, 0, 0, 0] }),
    ]),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C4 = `import { View, Text, Button, Layer, setProp } from 'kwikui';

Button({ text: '打开自由菜单', width: 140, height: 32, fontSize: 12,
         background: '#EEF2F7', color: '#334155', borderRadius: 6,
         onClick: () => setProp('menu-layer', 'active', 'true') });

// 自由模式：无 width / height / anchor → 全屏层，children 用 x / y 定位
Layer({ id: 'menu-layer', active: false, transparent: true }, [
    View({ x: 24, y: 210, width: 170, background: '#FFFFFF', borderRadius: 8,
           borderWidth: 1, borderColor: '#E2E8F0', padding: 4 }, [
        Text({ text: '新建项目', fontSize: 13, color: '#334155', padding: [8, 12], y: 4,
               onClick: () => setProp('menu-layer', 'active', 'false') }),
        Text({ text: '打开文件', fontSize: 13, color: '#334155', padding: [8, 12], y: 36,
               onClick: () => setProp('menu-layer', 'active', 'false') }),
        Text({ text: '退出', fontSize: 13, color: '#E5484D', padding: [8, 12], y: 68,
               onClick: () => setProp('menu-layer', 'active', 'false') }),
    ]),
]);`;

const demo4 = () => [
    Button({
        text: t(L.menu), width: 140, height: 32, fontSize: 12,
        background: '#EEF2F7', color: '#334155', borderRadius: 6,
        onClick: () => setProp('menu-layer', 'active', 'true'),
    }),
    Layer({ id: 'menu-layer', active: false, transparent: true }, [
        View({ x: 24, y: 210, width: 170, background: '#FFFFFF', borderRadius: 8,
               borderWidth: 1, borderColor: '#E2E8F0', padding: 4 }, [
            Text({ text: t(L.miNew), fontSize: 13, color: '#334155', padding: [8, 12], y: 4,
                   onClick: () => setProp('menu-layer', 'active', 'false') }),
            Text({ text: t(L.miOpen), fontSize: 13, color: '#334155', padding: [8, 12], y: 36,
                   onClick: () => setProp('menu-layer', 'active', 'false') }),
            Text({ text: t(L.miQuit), fontSize: 13, color: '#E5484D', padding: [8, 12], y: 68,
                   onClick: () => setProp('menu-layer', 'active', 'false') }),
        ]),
    ]),
    Text({ text: t(L.hint4), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    ...section(t(L.s3), demo3(), C3, innerW),
    ...section(t(L.s4), demo4(), C4, innerW),
    propsTable(t, meta.props, innerW),
];
