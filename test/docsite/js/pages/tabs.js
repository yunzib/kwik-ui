// ── Tabs 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/tabs.md。
// 三节结构镜像 test/ui/tabs.js（上位机验证可用的 demo）：
// ① 基本用法（等宽标签 + State 联动面板）② 自定义颜色 + 间距（tabSpacing>0，
// test/ui tabs3 同构）③ 命令式读写（getProp/setProp 按钮组镜像 test/ui 控制
// 段，读出经 State 显示）。demo 侧改动必须与本页演示同步（reverse 也一样）。
import { View, Flex, Text, Button, Tabs, State, getProp, setProp } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'tabs',
    group: 'data',
    title: { zh: 'Tabs 页签', en: 'Tabs' },
    desc: {
        zh: '横向标签条 + 内容面板切换：children 与 items 按索引一一对应，'
            + '只有选中面板参与布局和绘制；tabSpacing<=0 等宽平分、>0 自然宽度+间距；'
            + 'selectedIndex 内部自持（onChange 通报 {value,index}），可经 getProp/setProp 命令式读写。',
        en: 'Horizontal tab bar with content panels: children map to items by index and only the '
            + 'selected panel is laid out and drawn; tabSpacing<=0 makes tabs equal-width, >0 natural '
            + 'width + gaps. selectedIndex is self-held (onChange reports {value,index}); imperative '
            + 'read/write via getProp/setProp.',
    },
    props: [
        { title: { zh: '数据与值', en: 'Data & Value' }, rows: [
            ['items', 'string[]', '[]', { zh: '标签文字列表（与 children 对齐）', en: 'Tab labels (aligned with children)' }],
            ['selectedIndex', 'number', '0', { zh: '当前选中索引', en: 'Selected index' }],
            ['tabSpacing', 'number', '0', { zh: '标签间距（<=0 等宽）', en: 'Tab spacing (<=0 = equal width)' }],
            ['onChange', '(e) => {}', '-', { zh: '切换回调 e={value,index}', en: 'Change callback e={value,index}' }],
        ] },
        { title: { zh: '外观', en: 'Appearance' }, rows: [
            ['fontSize / indicatorHeight', 'number', '14 / 2', { zh: '标签字号 / 指示线高', en: 'Font / indicator height' }],
            ['tabColor / activeColor', '#rrggbb', '#666666 / #1976D2', { zh: '未选中 / 选中文字', en: 'Inactive / active text' }],
            ['tabBackground / activeTabBackground', '#rrggbb', '透明', { zh: '标签背景', en: 'Tab background' }],
            ['indicatorColor', '#rrggbb', '#1976D2', { zh: '指示线颜色', en: 'Indicator color' }],
        ] },
    ],
};

const L = {
    s1: { zh: '基本用法（等宽标签 + State 联动面板）', en: 'Basic (equal-width tabs + State-mirrored panels)' },
    s2: { zh: '自定义颜色 + 间距（tabSpacing>0）', en: 'Custom colors + spacing (tabSpacing>0)' },
    s3: { zh: '命令式读写（getProp / setProp）', en: 'Imperative read/write (getProp / setProp)' },
    overview: { zh: '概览', en: 'Overview' },
    metrics: { zh: '指标', en: 'Metrics' },
    logs: { zh: '日志', en: 'Logs' },
    redTheme: { zh: '红色主题', en: 'Red theme' },
    blueTheme: { zh: '蓝色主题', en: 'Blue theme' },
    greenTheme: { zh: '绿色主题', en: 'Green theme' },
    ovBody: { zh: '概览面板：关键数据与趋势摘要。', en: 'Overview panel: key numbers and trends.' },
    mtBody: { zh: '指标面板：转化率 4.8%，环比 +0.6%。', en: 'Metrics panel: conversion 4.8%, +0.6% WoW.' },
    lgBody: { zh: '日志面板：最近 24h 无异常。', en: 'Logs panel: no anomalies in the last 24h.' },
    pRed: { zh: '红色面板——activeTabBackground 高亮当前标签', en: 'Red panel — activeTabBackground highlights the active tab' },
    pBlue: { zh: '蓝色面板——指示线与选中文字同色（indicatorColor）', en: 'Blue panel — indicator and active text share one color' },
    pGreen: { zh: '绿色面板——标签间 8px 自然宽度间距', en: 'Green panel — 8px natural-width gaps between tabs' },
    selMetrics: { zh: '选中「指标」', en: 'Select "Metrics"' },
    selLogs: { zh: '选中「日志」', en: 'Select "Logs"' },
    getSel: { zh: 'getProp selectedIndex', en: 'getProp selectedIndex' },
    outEmpty: { zh: '（点上方按钮，读写结果显示这行）', en: '(Use the buttons above; results show on this line)' },
    setLine: { zh: 'setProp selectedIndex = ', en: 'setProp selectedIndex = ' },
    getLine: { zh: 'getProp selectedIndex = ', en: 'getProp selectedIndex = ' },
    hint1: { zh: 'selectedIndex 内部自持：点击标签直接生效并通报 onChange（e={value,index}），'
               + 'State 镜像即可联动其他元素；声明 selectedIndex 只定初值——受控回写门保证 '
               + 'rebuild 中声明未变更时不打回运行值。',
            en: 'selectedIndex is self-held: a tap applies instantly and reports onChange '
               + '(e={value,index}); mirror it into State to drive other elements. The declared '
               + 'selectedIndex is only the seed — the write-back gate keeps the runtime value on '
               + 'rebuild unless the declaration actually changed.' },
    hint2: { zh: 'tabSpacing>0 = 自然宽度 + 间距（不平分）；activeTabBackground 高亮当前标签，'
               + 'indicatorColor 画底线。本卡 selectedIndex 为声明初值 0——点击后的运行值在任意 '
               + 'rebuild 中保留（受控回写门）。',
            en: 'tabSpacing>0 = natural widths + gaps (not equal split); activeTabBackground '
               + 'highlights the active tab, indicatorColor draws the underline. This card declares '
               + 'selectedIndex: 0 as a seed — the tapped runtime value survives any rebuild '
               + '(write-back gate).' },
    hint3: { zh: 'getProp/setProp 走 PropBus，与点击同一条 setSelectedIndex 链路（setProp 同样'
               + '通报 onChange）；读写目标即本文第一节标签卡（id: ds-tabs）。',
            en: 'getProp/setProp go through PropBus on the same setSelectedIndex chain as taps '
               + '(setProp also reports onChange); the target is the Section-1 card (id: ds-tabs).' },
};

// ── 各节 State（st = §1 镜像读出；st3 = §3 命令式读出行；§2 无 State——
// 声明初值 + 点击自持，rebuild 由受控回写门保运行值）──
const st = new State({ index: 0 });
const st3 = new State({ out: '' });

// ── ① 基本用法（demo1 定稿保留形：State 联动读出）──
const demo1 = () => [
    Tabs({
        id: 'ds-tabs', items: [t(L.overview), t(L.metrics), t(L.logs)],
        selectedIndex: st.index, width: 420,
        fontSize: 13, activeColor: '#2563EB', indicatorColor: '#2563EB',
        tabColor: '#64748B', indicatorHeight: 2,
        onChange: (e) => { st.index = e.index; },
    }, [
        View({ width: 420, height: 100, background: '#EFF6FF', borderRadius: 8, padding: 14 }, [
            Text({ text: t(L.ovBody), fontSize: 12.5, color: '#1E40AF' }),
        ]),
        View({ width: 420, height: 100, background: '#F0FDF4', borderRadius: 8, padding: 14 }, [
            Text({ text: t(L.mtBody), fontSize: 12.5, color: '#166534' }),
        ]),
        View({ width: 420, height: 100, background: '#FFF7ED', borderRadius: 8, padding: 14 }, [
            Text({ text: t(L.lgBody), fontSize: 12.5, color: '#9A3412' }),
        ]),
    ]),
    Text({ text: 'selectedIndex = ' + st.index, fontSize: 12, color: '#2563EB', margin: [0, 9, 0, 0] }),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

// ── ② 自定义颜色 + 间距（test/ui tabs3 同构：selected 声明初值 + 点击自持）──
const demo2 = () => [
    Tabs({
        id: 'ds-tabs-2',
        items: [t(L.redTheme), t(L.blueTheme), t(L.greenTheme)],
        selectedIndex: 0,
        activeColor: '#FF5252',
        indicatorColor: '#FF5252',
        activeTabBackground: '#FFEBEE',
        tabSpacing: 8,
    }, [
        View({ background: '#FFEBEE', height: 80, borderRadius: 8, padding: 16 }, [
            Text({ text: t(L.pRed), fontSize: 16, color: '#C62828' }),
        ]),
        View({ background: '#E3F2FD', height: 80, borderRadius: 8, padding: 16 }, [
            Text({ text: t(L.pBlue), fontSize: 16, color: '#1565C0' }),
        ]),
        View({ background: '#E8F5E9', height: 80, borderRadius: 8, padding: 16 }, [
            Text({ text: t(L.pGreen), fontSize: 16, color: '#2E7D32' }),
        ]),
    ]),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

// ── ③ 命令式读写（test/ui 控制段镜像；读写目标 = §1 的 ds-tabs；读出经 State）──
const demo3 = () => [
    Flex({ gap: 8 }, [
        Button({ text: t(L.selMetrics), flexGrow: 1, height: 36, borderRadius: 8,
            background: '#FFFFFF', color: '#0F172A', borderWidth: 1, borderColor: '#E2E8F0', fontSize: 12,
            onClick: () => { setProp('ds-tabs', 'selectedIndex', '1'); st3.out = t(L.setLine) + '1'; } }),
        Button({ text: t(L.selLogs), flexGrow: 1, height: 36, borderRadius: 8,
            background: '#FFFFFF', color: '#0F172A', borderWidth: 1, borderColor: '#E2E8F0', fontSize: 12,
            onClick: () => { setProp('ds-tabs', 'selectedIndex', '2'); st3.out = t(L.setLine) + '2'; } }),
        Button({ text: t(L.getSel), flexGrow: 1, height: 36, borderRadius: 8,
            background: '#FFFFFF', color: '#0F172A', borderWidth: 1, borderColor: '#E2E8F0', fontSize: 12,
            onClick: () => { st3.out = t(L.getLine) + getProp('ds-tabs', 'selectedIndex'); } }),
    ]),
    Text({ text: st3.out || t(L.outEmpty), fontSize: 12, color: '#2563EB', margin: [10, 0, 0, 0] }),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

// 代码卡（与上方演示逐行同源；改动演示同步这里）
const C1 = `// 基本联动：selectedIndex 内部自持 + onChange 通报 → State 镜像 → 读出联动
const st = new State({ index: 0 });

Tabs({
    id: 'ds-tabs',
    items: ['概览', '指标', '日志'],
    selectedIndex: st.index,          // State 读随重扩展翻转；点击后运行值经回写门保留
    width: 420,
    fontSize: 13, activeColor: '#2563EB', indicatorColor: '#2563EB',
    tabColor: '#64748B', indicatorHeight: 2,
    onChange: (e) => { st.index = e.index; },
}, [
    View({ width: 420, height: 100, background: '#EFF6FF', borderRadius: 8, padding: 14 },
         [Text({ text: '概览面板：关键数据与趋势摘要。', fontSize: 12.5, color: '#1E40AF' })]),
    View({ width: 420, height: 100, background: '#F0FDF4', borderRadius: 8, padding: 14 },
         [Text({ text: '指标面板：转化率 4.8%，环比 +0.6%。', fontSize: 12.5, color: '#166534' })]),
    View({ width: 420, height: 100, background: '#FFF7ED', borderRadius: 8, padding: 14 },
         [Text({ text: '日志面板：最近 24h 无异常。', fontSize: 12.5, color: '#9A3412' })]),
]);

Text({ text: 'selectedIndex = ' + st.index });   // 联动读出`;

const C2 = `// 自定义颜色 + 间距：tabSpacing>0 = 自然宽度 + 间距（activeTabBackground
// 高亮当前标签）。selectedIndex 声明只是初值——点击后的运行值经受控回写门
// 在每次 rebuild 中保留（声明未变更不打回）
Tabs({
    id: 'ds-tabs-2',
    items: ['红色主题', '蓝色主题', '绿色主题'],
    selectedIndex: 0,
    activeColor: '#FF5252',
    indicatorColor: '#FF5252',
    activeTabBackground: '#FFEBEE',
    tabSpacing: 8,
}, [
    View({ background: '#FFEBEE', height: 80, borderRadius: 8, padding: 16 },
         [Text({ text: '红色面板', fontSize: 16, color: '#C62828' })]),
    View({ background: '#E3F2FD', height: 80, borderRadius: 8, padding: 16 },
         [Text({ text: '蓝色面板', fontSize: 16, color: '#1565C0' })]),
    View({ background: '#E8F5E9', height: 80, borderRadius: 8, padding: 16 },
         [Text({ text: '绿色面板', fontSize: 16, color: '#2E7D32' })]),
]);`;

const C3 = `// 命令式读写：getProp / setProp 走 PropBus——与点击同一条 setSelectedIndex
// 链路（setProp 同样通报 onChange；读取即时返回运行值）；out.write 触发
// rebuild 时被写的标签卡声明未变更 → 运行值经回写门保留
const out = new State({ text: '' });

Flex({ gap: 8 }, [
    Button({ text: '选中「指标」', flexGrow: 1, height: 36, borderRadius: 8,
             background: '#FFFFFF', color: '#0F172A',
             borderWidth: 1, borderColor: '#E2E8F0', fontSize: 12,
             onClick: () => { setProp('ds-tabs', 'selectedIndex', '1');
                              out.text = 'setProp selectedIndex = 1'; } }),
    Button({ text: '选中「日志」', flexGrow: 1, height: 36, borderRadius: 8,
             background: '#FFFFFF', color: '#0F172A',
             borderWidth: 1, borderColor: '#E2E8F0', fontSize: 12,
             onClick: () => { setProp('ds-tabs', 'selectedIndex', '2');
                              out.text = 'setProp selectedIndex = 2'; } }),
    Button({ text: 'getProp selectedIndex', flexGrow: 1, height: 36, borderRadius: 8,
             background: '#FFFFFF', color: '#0F172A',
             borderWidth: 1, borderColor: '#E2E8F0', fontSize: 12,
             onClick: () => { out.text = 'getProp selectedIndex = '
                                       + getProp('ds-tabs', 'selectedIndex'); } }),
]);

Text({ text: out.text || '（点上方按钮，读写结果显示这行）' });`;

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    ...section(t(L.s3), demo3(), C3, innerW),
    propsTable(t, meta.props, innerW),
];
