// ── StackIndex 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/stackindex.md。
// 单演示小节：按钮切页 + 三面板。
import { View, Flex, Text, Button, StackIndex, State } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'stackindex',
    group: 'layout',
    title: { zh: 'StackIndex 页栈', en: 'StackIndex' },
    desc: {
        zh: '按 index 切换内容面板，仅选中面板参与测量 / 布局 / 绘制；'
            + '与 Tabs 的内容区语义一致，但不带标签条。',
        en: 'Switches panels by index; only the selected child is measured, laid out and drawn. '
            + 'Same content model as Tabs, without the tab bar.',
    },
    props: [
        { title: { zh: '专有属性', en: 'Props' }, rows: [
            ['index', 'number', '0', { zh: '当前面板索引（越界 = 隐藏全部）', en: 'Panel index (out of range = hide all)' }],
            ['onChange', '(e) => {}', '-', { zh: '切换回调 e={index}', en: 'Change callback e={index}' }],
        ] },
    ],
};

const L = {
    s1: { zh: '面板切换', en: 'Panel switch' },
    panelA: { zh: '面板 A', en: 'Panel A' },
    panelB: { zh: '面板 B', en: 'Panel B' },
    panelC: { zh: '面板 C', en: 'Panel C' },
};

const st = new State({ index: 0 });

const C1 = `const nav = new State({ index: 0 });

StackIndex({ index: nav.index, width: 420, height: 120 }, [
    View({ background: '#DBEAFE' }, [Text({ text: '面板 A' })]),
    View({ background: '#DCFCE7' }, [Text({ text: '面板 B' })]),
]);
// 也可以用 setProp('stack', 'index', '1') 切换`;
const demo1 = () => [
    Flex({ direction: 'row', gap: 8, margin: [0, 0, 10, 0] }, [0, 1, 2].map(i => Button({
        text: t([L.panelA, L.panelB, L.panelC][i]),
        width: 92, height: 30, fontSize: 12,
        background: st.index === i ? '#1E3A8A' : '#EEF2F7',
        color: st.index === i ? '#FFFFFF' : '#334155', borderRadius: 6,
        onClick: () => { st.index = i; },
    }))),
    StackIndex({ id: 'ds-stackindex', index: st.index, width: 420, height: 120 }, [
        View({ width: 420, height: 120, background: '#DBEAFE', borderRadius: 8, padding: 16 }, [
            Text({ text: '面板 A · 基础信息', fontSize: 14, color: '#1E40AF' }),
        ]),
        View({ width: 420, height: 120, background: '#DCFCE7', borderRadius: 8, padding: 16 }, [
            Text({ text: '面板 B · 高级设置', fontSize: 14, color: '#166534' }),
        ]),
        View({ width: 420, height: 120, background: '#FEE2E2', borderRadius: 8, padding: 16 }, [
            Text({ text: '面板 C · 危险操作', fontSize: 14, color: '#991B1B' }),
        ]),
    ]),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
