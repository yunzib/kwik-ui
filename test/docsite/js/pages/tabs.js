// ── Tabs 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/tabs.md。
// 单演示小节：标签条 + 三面板切换。
import { View, Flex, Text, Tabs, State } from 'kwikui';
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
            + '只有选中面板参与布局和绘制；tabSpacing<=0 时标签等宽平分。',
        en: 'Horizontal tab bar with content panels: children map to items by index and only the '
            + 'selected panel is laid out and drawn. tabSpacing<=0 makes tabs equal-width.',
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
    s1: { zh: '标签 + 面板', en: 'Tabs + panels' },
    overview: { zh: '概览', en: 'Overview' },
    metrics: { zh: '指标', en: 'Metrics' },
    logs: { zh: '日志', en: 'Logs' },
};

const st = new State({ index: 0 });

const C1 = `Tabs({
    items: ['概览', '指标', '日志'],
    selectedIndex: 0,
    onChange: (e) => console.log(e.index, e.value),
}, [
    View({ height: 100 }, [Text({ text: '概览面板' })]),
    View({ height: 100 }, [Text({ text: '指标面板' })]),
    View({ height: 100 }, [Text({ text: '日志面板' })]),
]);`;
const demo1 = () => [
    Tabs({
        id: 'ds-tabs', items: [t(L.overview), t(L.metrics), t(L.logs)],
        selectedIndex: st.index, width: 420,
        fontSize: 13, activeColor: '#2563EB', indicatorColor: '#2563EB',
        tabColor: '#64748B', indicatorHeight: 2,
        onChange: (e) => { st.index = e.index; },
    }, [
        View({ width: 420, height: 100, background: '#EFF6FF', borderRadius: 8, padding: 14 }, [
            Text({ text: '概览面板：关键数据与趋势摘要。', fontSize: 12.5, color: '#1E40AF' }),
        ]),
        View({ width: 420, height: 100, background: '#F0FDF4', borderRadius: 8, padding: 14 }, [
            Text({ text: '指标面板：转化率 4.8%，环比 +0.6%。', fontSize: 12.5, color: '#166534' }),
        ]),
        View({ width: 420, height: 100, background: '#FFF7ED', borderRadius: 8, padding: 14 }, [
            Text({ text: '日志面板：最近 24h 无异常。', fontSize: 12.5, color: '#9A3412' }),
        ]),
    ]),
    Text({ text: 'selectedIndex = ' + st.index, fontSize: 12, color: '#2563EB', margin: [0, 9, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
