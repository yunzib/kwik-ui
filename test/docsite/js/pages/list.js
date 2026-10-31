// ── List 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/list.md。
// 四个示例小节：垂直堆叠与分割线 / 水平列表 / 固定 header·footer / 命令式滚动。
// 示例代码与演示同构：C1–C4 逐行镜像 demo1–demo4（改演示必须同步改代码，
// 反之亦然；同 flex / grid / stack 页纪律）。
// 注意：ListLayout 不消费 gap——行距只能用 dividerHeight 或子项 margin，
// 属性表与演示均不再出现 gap。
import { View, Flex, Text, Button, List, setProp } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'list',
    group: 'layout',
    title: { zh: 'List 列表', en: 'List' },
    desc: {
        zh: '单轴滚动列表：子项沿方向堆叠（垂直模式行宽自动拉满内容区），滚轮自动滚动、'
            + '到边界交外层容器；dividerHeight / dividerColor 画行间分割线（兼作行距；'
            + 'List 不消费 gap，也可用子项 margin 留距）；header / footer 为固定在滚动区外的'
            + '首尾栏；不写 height 时高度 = 内容 + 首尾栏 + padding；setProp scrollX / scrollY 命令式定位。',
        en: 'Single-axis scrolling list: children stack along the axis (vertical mode stretches row '
            + 'width to the content area); the wheel scrolls automatically and hands off to ancestor '
            + 'scrollers at the edge; dividerHeight / dividerColor draw separators between adjacent '
            + 'rows (doubling as row spacing — gap is NOT consumed; use item margins too); header / '
            + 'footer are fixed bars outside the scroll region; without height the list sizes to '
            + 'content + bars + padding; setProp scrollX / scrollY position imperatively.',
    },
    props: [
        { title: { zh: '专有属性', en: 'Props' }, rows: [
            ['scrollDirection', '"vertical" | "horizontal" | "both"', '"vertical"', { zh: '滚动方向', en: 'Scroll axis' }],
            ['dividerColor / dividerHeight', '#rrggbb / number', '透明 / 0', { zh: '行间分割线（只画在相邻行之间），兼作行距', en: 'Separators between adjacent rows (double as spacing)' }],
            ['header / footer', '元素值', '-', { zh: '固定首 / 尾栏（滚动区外，作为属性传入而非子项）', en: 'Fixed bars outside the scroll region (props, not children)' }],
        ] },
        { title: { zh: '尺寸（通用）', en: 'Sizing' }, rows: [
            ['width / height', 'number | 百分比', '-', { zh: '显式尺寸；不写 height（且父无界）时 = 内容高 + 首尾栏 + padding，不再内滚', en: 'Explicit size; height falls back to content + bars + padding' }],
        ] },
        { title: { zh: '命令式', en: 'Imperative' }, rows: [
            ['setProp(id, "scrollX" / "scrollY", v)', '-', '-', { zh: '滚动定位（超界自动 clamp，传大数即贴底）', en: 'Scroll positioning (clamped; large value = bottom)' }],
        ] },
    ],
};

const L = {
    s1: { zh: '垂直堆叠与分割线', en: 'Vertical stack & dividers' },
    s2: { zh: '水平列表', en: 'Horizontal list' },
    s3: { zh: '固定 header / footer', en: 'Fixed header & footer' },
    s4: { zh: '命令式滚动', en: 'Imperative scrolling' },
    hd: { zh: '消息中心', en: 'Inbox' },
    ft: { zh: '共 10 条', en: '10 items total' },
    row: { zh: '行', en: 'Row' },
    toTop: { zh: '回顶部', en: 'Top' },
    toMid: { zh: '中部', en: 'Middle' },
    toEnd: { zh: '贴底', en: 'Bottom' },
    hint1: { zh: '垂直模式子项宽度自动拉满内容区、高度按内容测量；分割线只画在相邻行之间——'
            + 'dividerHeight 兼作行距（List 不消费 gap，更大间距也可用子项 margin）；'
            + '内容超出容器高后滚轮自动滚动。',
             en: 'Vertical mode stretches row width to the content area and measures height per '
             + 'row; separators are drawn only between adjacent rows — dividerHeight doubles as row '
             + 'spacing (gap is NOT consumed; item margins work too); once content overflows, the '
             + 'wheel scrolls automatically' },
    hint2: { zh: 'scrollDirection:"horizontal" 后子项横向堆叠、高度拉满内容区；'
            + '分割线变为竖线（宽 = dividerHeight），超出容器宽后横向滚动。',
             en: 'With scrollDirection:"horizontal" children stack sideways with height stretched '
             + 'to the content area; separators become vertical lines (width = dividerHeight) and '
             + 'the list scrolls horizontally once content overflows' },
    hint3: { zh: 'header / footer 是属性值（各传一个元素），固定在滚动区外不随内容滚动；'
            + '内容可视高 = 容器高 − 首栏 − 尾栏。',
             en: 'header / footer are prop values (one element each), fixed outside the scroll '
             + 'region; the visible content height = container − header − footer' },
    hint4: { zh: '滚轮 / 触摸板滚动由容器自动处理（到边界交给外层滚动容器继续）；'
            + '命令式定位用 setProp 写 scrollX / scrollY，超界值自动 clamp——传一个大数即贴底。',
             en: 'Wheel scrolling is handled automatically (edge hands off to ancestor scrollers); '
             + 'imperative positioning writes scrollX / scrollY via setProp — values are clamped, '
             + 'so a large number jumps to the end' },
};

const C1 = `import { Flex, Text, List } from 'kwikui';

// 垂直堆叠：行宽自动拉满内容区；分割线只画在相邻行之间（兼作行距）
List({
    width: 420, height: 190,
    dividerHeight: 1, dividerColor: '#F1F5F9',
    background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8, padding: 8,
}, Array.from({ length: 8 }, (_, i) => Flex({
    direction: 'row', alignItems: 'center', justifyContent: 'spaceBetween',
    height: 34, padding: [0, 10, 0, 10], borderRadius: 5,
    background: i % 2 ? '#F8FAFC' : '#FFFFFF',
}, [
    Text({ text: '列表项 ' + (i + 1), fontSize: 12.5, color: '#334155' }),
    Text({ text: '›', fontSize: 14, color: '#94A3B8' }),
])));`;

const demo1 = () => [
    List({
        width: 420, height: 190,
        dividerHeight: 1, dividerColor: '#F1F5F9',
        background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8, padding: 8,
    }, Array.from({ length: 8 }, (_, i) => Flex({
        direction: 'row', alignItems: 'center', justifyContent: 'spaceBetween',
        height: 34, padding: [0, 10, 0, 10], borderRadius: 5,
        background: i % 2 ? '#F8FAFC' : '#FFFFFF',
    }, [
        Text({ text: '列表项 ' + (i + 1), fontSize: 12.5, color: '#334155' }),
        Text({ text: '›', fontSize: 14, color: '#94A3B8' }),
    ]))),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C2 = `import { View, Text, List } from 'kwikui';

// 水平列表：子项横向堆叠、高度拉满内容区；分割线变竖线
List({
    scrollDirection: 'horizontal', width: 420, height: 96,
    dividerHeight: 1, dividerColor: '#F1F5F9',
    background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8, padding: 8,
}, Array.from({ length: 8 }, (_, i) => View({
    width: 108, borderRadius: 6,
    background: ['#DBEAFE', '#DCFCE7', '#FEE2E2', '#FEF3C7'][i % 4],
}, [Text({ text: 'card ' + (i + 1), fontSize: 11.5, color: '#334155', padding: [8, 0, 0, 10] })])));`;

const demo2 = () => [
    List({
        scrollDirection: 'horizontal', width: 420, height: 96,
        dividerHeight: 1, dividerColor: '#F1F5F9',
        background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8, padding: 8,
    }, Array.from({ length: 8 }, (_, i) => View({
        width: 108, borderRadius: 6,
        background: ['#DBEAFE', '#DCFCE7', '#FEE2E2', '#FEF3C7'][i % 4],
    }, [Text({ text: 'card ' + (i + 1), fontSize: 11.5, color: '#334155', padding: [8, 0, 0, 10] })]))),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C3 = `import { Flex, Text, List } from 'kwikui';

// header / footer 是属性值（各一个元素），固定在滚动区外
List({
    width: 420, height: 210,
    dividerHeight: 1, dividerColor: '#F1F5F9',
    background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8,
    header: Flex({ direction: 'row', alignItems: 'center', height: 36,
                   padding: [0, 12, 0, 12], background: '#F1F5F9' },
                 [Text({ text: '消息中心', fontSize: 12.5, fontWeight: 'bold', color: '#0F172A' })]),
    footer: Flex({ direction: 'row', alignItems: 'center', justifyContent: 'end', height: 32,
                   padding: [0, 12, 0, 12], background: '#F8FAFC' },
                 [Text({ text: '共 10 条', fontSize: 11.5, color: '#64748B' })]),
}, Array.from({ length: 10 }, (_, i) => Flex({
    direction: 'row', alignItems: 'center', height: 30, padding: [0, 12, 0, 12],
}, [Text({ text: '行 ' + (i + 1), fontSize: 12, color: '#334155' })])));`;

const demo3 = () => [
    List({
        width: 420, height: 210,
        dividerHeight: 1, dividerColor: '#F1F5F9',
        background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8,
        header: Flex({ direction: 'row', alignItems: 'center', height: 36,
                       padding: [0, 12, 0, 12], background: '#F1F5F9' },
                     [Text({ text: t(L.hd), fontSize: 12.5, fontWeight: 'bold', color: '#0F172A' })]),
        footer: Flex({ direction: 'row', alignItems: 'center', justifyContent: 'end', height: 32,
                       padding: [0, 12, 0, 12], background: '#F8FAFC' },
                     [Text({ text: t(L.ft), fontSize: 11.5, color: '#64748B' })]),
    }, Array.from({ length: 10 }, (_, i) => Flex({
        direction: 'row', alignItems: 'center', height: 30, padding: [0, 12, 0, 12],
    }, [Text({ text: t(L.row) + ' ' + (i + 1), fontSize: 12, color: '#334155' })]))),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C4 = `import { Flex, Text, Button, List, setProp } from 'kwikui';

// 滚轮滚动由容器自动处理；命令式定位经 setProp 写 scrollY（自动 clamp，大数即贴底）
Flex({ direction: 'row', gap: 10 }, [
    List({
        id: 'imp-list', width: 420, height: 150,
        dividerHeight: 1, dividerColor: '#F1F5F9',
        background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8,
    }, Array.from({ length: 20 }, (_, i) => Flex({
        direction: 'row', alignItems: 'center', height: 28, padding: [0, 12, 0, 12],
    }, [Text({ text: '行 ' + (i + 1), fontSize: 12, color: '#334155' })]))),
    Flex({ direction: 'column', gap: 8 }, [
        Button({ text: '回顶部', width: 88, height: 30, fontSize: 12,
                 background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
                 onClick: () => setProp('imp-list', 'scrollY', 0) }),
        Button({ text: '中部', width: 88, height: 30, fontSize: 12,
                 background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
                 onClick: () => setProp('imp-list', 'scrollY', 140) }),
        Button({ text: '贴底', width: 88, height: 30, fontSize: 12,
                 background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
                 onClick: () => setProp('imp-list', 'scrollY', 10000) }),
    ]),
]);`;

const demo4 = () => [
    Flex({ direction: 'row', gap: 10, alignItems: 'flexStart' }, [
        List({
            id: 'imp-list', width: 420, height: 150,
            dividerHeight: 1, dividerColor: '#F1F5F9',
            background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8,
        }, Array.from({ length: 20 }, (_, i) => Flex({
            direction: 'row', alignItems: 'center', height: 28, padding: [0, 12, 0, 12],
        }, [Text({ text: t(L.row) + ' ' + (i + 1), fontSize: 12, color: '#334155' })]))),
        Flex({ direction: 'column', gap: 8 }, [
            Button({ text: t(L.toTop), width: 88, height: 30, fontSize: 12,
                     background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
                     onClick: () => setProp('imp-list', 'scrollY', 0) }),
            Button({ text: t(L.toMid), width: 88, height: 30, fontSize: 12,
                     background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
                     onClick: () => setProp('imp-list', 'scrollY', 140) }),
            Button({ text: t(L.toEnd), width: 88, height: 30, fontSize: 12,
                     background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
                     onClick: () => setProp('imp-list', 'scrollY', 10000) }),
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
