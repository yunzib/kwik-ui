// ── LazyList 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/lazylist.md。
// 四个示例小节：固定模式 / 可变行高 / header·footer / 横向虚拟化。
// 示例代码与演示同构：C1–C4 逐行镜像 demo1–demo4（改演示必须同步改代码，
// 反之亦然；同 flex / grid / stack / list / scrollview 页纪律）。
import { View, Flex, Text, LazyList } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'lazylist',
    group: 'layout',
    title: { zh: 'LazyList 惰性列表', en: 'LazyList' },
    desc: {
        zh: '大数据集虚拟化列表：只构建视口 + overscan 内的行，滚动时按 index 做窗口 diff，'
            + '首帧与滚动开销恒为 O(窗口)，适配千 / 万行数据。固定模式（itemHeight / itemWidth）'
            + '零测量 O(1) 定位；可变模式 estimatedItemSize 兜底 + 实测缓存收敛；'
            + 'header / footer 固定在滚动区外（同 List）。',
        en: 'Virtualized list for large datasets: builds only viewport + overscan rows and diffs '
            + 'the window by index, keeping first-frame and scroll cost at O(window). Fixed mode '
            + '(itemHeight / itemWidth) positions in O(1) with zero measuring; variable mode falls '
            + 'back to estimatedItemSize then converges with measured caching; header / footer are '
            + 'fixed outside the scroll region (same as List).',
    },
    props: [
        { title: { zh: '数据与构建', en: 'Data & Builder' }, rows: [
            ['items', 'Array', '[]', { zh: '数据数组（必需）', en: 'Data array (required)' }],
            ['itemBuilder', '(item,index) => View', '-', { zh: '行内容构建回调（必需）', en: 'Row builder (required)' }],
            ['direction', '"vertical" | "horizontal"', '"vertical"', { zh: '滚动主轴', en: 'Scroll axis' }],
        ] },
        { title: { zh: '虚拟化', en: 'Virtualization' }, rows: [
            ['itemHeight', 'number', '0', { zh: '纵向固定行高（>0 启用固定模式）', en: 'Fixed row height for vertical mode' }],
            ['itemWidth', 'number', '0', { zh: '横向固定行宽', en: 'Fixed row width for horizontal mode' }],
            ['estimatedItemSize', 'number', '40', { zh: '可变模式未实测行估计长度', en: 'Estimated size in variable mode' }],
            ['overscan', 'number', '2', { zh: '视口外预构建行数', en: 'Extra rows built outside viewport' }],
        ] },
        { title: { zh: '装饰', en: 'Decoration' }, rows: [
            ['dividerHeight / dividerColor', 'number / #rrggbb', '0 / 透明', { zh: '行分割线', en: 'Row divider' }],
            ['header / footer', 'View', '-', { zh: '滚动区外固定头 / 尾', en: 'Fixed header / footer' }],
        ] },
    ],
};

const L = {
    s1: { zh: '固定模式虚拟化', en: 'Fixed-mode virtualization' },
    s2: { zh: '可变行高模式', en: 'Variable heights' },
    s3: { zh: '固定 header / footer', en: 'Fixed header & footer' },
    s4: { zh: '横向虚拟化', en: 'Horizontal virtualization' },
    hd: { zh: '联系人（1000 项）', en: 'Contacts (1000)' },
    ft: { zh: '—— 到底了 ——', en: '— end of list —' },
    rowN: { zh: '行', en: 'Row' },
    hint1: { zh: '固定模式：itemHeight > 0 → 所有行等长，定位 O(1)、零测量。数据 500 行，'
            + '实际只构建视口 + 双向 overscan 内的行（约 9 行），滚动时按 index 做窗口 diff'
            + '（在窗行复用、出窗行 discard、缺行 itemBuilder 补建）。',
             en: 'Fixed mode: itemHeight > 0 makes all rows equal — O(1) positioning, zero '
             + 'measuring. 500 rows of data but only the viewport + overscan window (about 9 '
             + 'rows) is ever built; scrolling diffs the window by index (in-window rows are '
             + 'reused, out-of-window rows discarded, missing rows built by itemBuilder)' },
    hint2: { zh: '可变模式：不传 itemHeight → estimatedItemSize 兜底 + 实测行高缓存 + 前缀和；'
            + '窗口行逐个实测、滚过即收敛（行高 40/52/64/76/88 交替，滚动中逐步精确）。',
             en: 'Variable mode: without itemHeight, estimatedItemSize seeds the sizes and measured '
             + 'heights cache into a prefix sum; rows in the window are measured as they appear '
             + 'and the layout converges (heights alternate 40/52/64/76/88)' },
    hint3: { zh: 'header / footer 各传一个元素，固定在滚动区外不随主轴滚动（同 List）；'
            + '内容可视高 = 容器高 − 首栏 − 尾栏。',
             en: 'header / footer take one element each, fixed outside the scroll region (same as '
             + 'List); visible content height = container − header − footer' },
    hint4: { zh: 'direction:"horizontal" + itemWidth 固定行宽，横向虚拟化滚动；'
            + '已知取舍：v1 无滚动条（滚轮 / 触控板滚动可用）。',
             en: 'direction:"horizontal" with itemWidth virtualizes sideways; known trade-off: '
             + 'v1 has no scrollbar (wheel / trackpad scrolling works)' },
};

const C1 = `import { Flex, Text, LazyList } from 'kwikui';

// 固定模式：itemHeight=40，只构建视口 + overscan 窗口（数据 500 行）
LazyList({
    width: 420, height: 210,
    direction: 'vertical', itemHeight: 40, overscan: 3,
    dividerHeight: 1, dividerColor: '#F1F5F9',
    background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8,
    items: Array.from({ length: 500 }, (_, i) => i),
    itemBuilder: (item, index) => Flex({
        direction: 'row', alignItems: 'center', justifyContent: 'spaceBetween',
        height: 40, padding: [0, 12, 0, 12],
        background: index % 2 ? '#F8FAFC' : '#FFFFFF',
    }, [
        Text({ text: '第 ' + index + ' 行', fontSize: 12, color: '#334155' }),
        Text({ text: '# ' + item, fontSize: 11, color: '#94A3B8' }),
    ]),
});`;

const demo1 = () => [
    LazyList({
        width: 420, height: 210,
        direction: 'vertical', itemHeight: 40, overscan: 3,
        dividerHeight: 1, dividerColor: '#F1F5F9',
        background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8,
        items: Array.from({ length: 500 }, (_, i) => i),
        itemBuilder: (item, index) => Flex({
            direction: 'row', alignItems: 'center', justifyContent: 'spaceBetween',
            height: 40, padding: [0, 12, 0, 12],
            background: index % 2 ? '#F8FAFC' : '#FFFFFF',
        }, [
            Text({ text: '第 ' + index + ' 行', fontSize: 12, color: '#334155' }),
            Text({ text: '# ' + item, fontSize: 11, color: '#94A3B8' }),
        ]),
    }),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C2 = `import { View, Text, LazyList } from 'kwikui';

// 可变模式：不传 itemHeight，行高 40~88 交替（实测缓存，滚过即收敛）
LazyList({
    width: 420, height: 210,
    direction: 'vertical', estimatedItemSize: 48, overscan: 2,
    background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8,
    items: Array.from({ length: 300 }, (_, i) => i),
    itemBuilder: (item, index) => {
        const h = 40 + (index % 5) * 12;    // 40 / 52 / 64 / 76 / 88 交替
        return View({ height: h, background: index % 2 ? '#F0F9FF' : '#FFFFFF' }, [
            Text({ text: '可变行 ' + index + ' · 高 ' + h, fontSize: 12, color: '#334155',
                   padding: [8, 0, 0, 12] }),
        ]);
    },
});`;

const demo2 = () => [
    LazyList({
        width: 420, height: 210,
        direction: 'vertical', estimatedItemSize: 48, overscan: 2,
        background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8,
        items: Array.from({ length: 300 }, (_, i) => i),
        itemBuilder: (item, index) => {
            const h = 40 + (index % 5) * 12;    // 40 / 52 / 64 / 76 / 88 交替
            return View({ height: h, background: index % 2 ? '#F0F9FF' : '#FFFFFF' }, [
                Text({ text: '可变行 ' + index + ' · 高 ' + h, fontSize: 12, color: '#334155',
                       padding: [8, 0, 0, 12] }),
            ]);
        },
    }),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C3 = `import { Flex, Text, LazyList } from 'kwikui';

// header / footer：各传一个元素，固定在滚动区外（同 List）
LazyList({
    width: 420, height: 220,
    direction: 'vertical', itemHeight: 36,
    dividerHeight: 1, dividerColor: '#F1F5F9',
    background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8,
    header: Flex({ direction: 'row', alignItems: 'center', height: 34,
                   padding: [0, 12, 0, 12], background: '#F1F5F9' },
                 [Text({ text: '联系人（1000 项）', fontSize: 12.5, fontWeight: 'bold', color: '#0F172A' })]),
    footer: Flex({ direction: 'row', alignItems: 'center', justifyContent: 'center', height: 30,
                   padding: [0, 12, 0, 12], background: '#F8FAFC' },
                 [Text({ text: '—— 到底了 ——', fontSize: 11.5, color: '#94A3B8' })]),
    items: Array.from({ length: 1000 }, (_, i) => i),
    itemBuilder: (item, index) => Flex({
        direction: 'row', alignItems: 'center', justifyContent: 'spaceBetween',
        height: 36, padding: [0, 12, 0, 12],
    }, [
        Text({ text: '行 ' + index, fontSize: 12, color: '#334155' }),
        Text({ text: '# ' + item, fontSize: 11, color: '#94A3B8' }),
    ]),
});`;

const demo3 = () => [
    LazyList({
        width: 420, height: 220,
        direction: 'vertical', itemHeight: 36,
        dividerHeight: 1, dividerColor: '#F1F5F9',
        background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8,
        header: Flex({ direction: 'row', alignItems: 'center', height: 34,
                       padding: [0, 12, 0, 12], background: '#F1F5F9' },
                     [Text({ text: t(L.hd), fontSize: 12.5, fontWeight: 'bold', color: '#0F172A' })]),
        footer: Flex({ direction: 'row', alignItems: 'center', justifyContent: 'center', height: 30,
                       padding: [0, 12, 0, 12], background: '#F8FAFC' },
                     [Text({ text: t(L.ft), fontSize: 11.5, color: '#94A3B8' })]),
        items: Array.from({ length: 1000 }, (_, i) => i),
        itemBuilder: (item, index) => Flex({
            direction: 'row', alignItems: 'center', justifyContent: 'spaceBetween',
            height: 36, padding: [0, 12, 0, 12],
        }, [
            Text({ text: t(L.rowN) + ' ' + index, fontSize: 12, color: '#334155' }),
            Text({ text: '# ' + item, fontSize: 11, color: '#94A3B8' }),
        ]),
    }),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C4 = `import { View, Text, LazyList } from 'kwikui';

// 横向虚拟化：direction + itemWidth 固定行宽（数据 200 项）
LazyList({
    width: 420, height: 96,
    direction: 'horizontal', itemWidth: 120, overscan: 2,
    dividerHeight: 1, dividerColor: '#F1F5F9',
    background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8, padding: 8,
    items: Array.from({ length: 200 }, (_, i) => i),
    itemBuilder: (item, index) => View({
        width: 120, height: 78, borderRadius: 6,
        background: ['#DBEAFE', '#DCFCE7', '#FEE2E2', '#FEF3C7'][index % 4],
    }, [Text({ text: 'card ' + index, fontSize: 11.5, color: '#334155', padding: [8, 0, 0, 10] })]),
});`;

const demo4 = () => [
    LazyList({
        width: 420, height: 96,
        direction: 'horizontal', itemWidth: 120, overscan: 2,
        dividerHeight: 1, dividerColor: '#F1F5F9',
        background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8, padding: 8,
        items: Array.from({ length: 200 }, (_, i) => i),
        itemBuilder: (item, index) => View({
            width: 120, height: 78, borderRadius: 6,
            background: ['#DBEAFE', '#DCFCE7', '#FEE2E2', '#FEF3C7'][index % 4],
        }, [Text({ text: 'card ' + index, fontSize: 11.5, color: '#334155', padding: [8, 0, 0, 10] })]),
    }),
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
