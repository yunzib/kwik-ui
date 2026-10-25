// ── Dropdown 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/dropdown.md。
// 单演示小节：下拉菜单 + ref 绑定 + 选中项回显。
import { View, Flex, Text, Dropdown, State, ref } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'dropdown',
    group: 'form',
    title: { zh: 'Dropdown 下拉选择', en: 'Dropdown' },
    desc: {
        zh: '点击触发区展开菜单，选中项高亮回显；菜单是独立浮层，'
            + '不会被祖先裁剪，点击外部或再次点击触发区自动收起。',
        en: 'Tap to open a menu with highlighted selection; the menu is an independent layer, '
            + 'not clipped by ancestors, and closes on outside tap or trigger tap.',
    },
    props: [
        { title: { zh: '数据与值', en: 'Data & Value' }, rows: [
            ['items', 'string[]', '[]', { zh: '选项列表', en: 'Options' }],
            ['value', 'string | ref(state,key)', "''", { zh: '当前值 / 双向绑定', en: 'Value / two-way binding' }],
            ['selectedIndex', 'number', '-1', { zh: '选中索引（-1=未选）', en: 'Selected index (-1 = none)' }],
            ['placeholder', 'string', '请选择...', { zh: '未选择时占位', en: 'Placeholder when empty' }],
            ['onChange', '(e) => {}', '-', { zh: '回调 e={value,index}', en: 'Callback e={value,index}' }],
        ] },
        { title: { zh: '外观与菜单', en: 'Style & Menu' }, rows: [
            ['fontSize', 'number', '14', { zh: '字号 px', en: 'Font size' }],
            ['itemHeight / maxVisibleItems', 'number', '20 / 5', { zh: '选项高 / 可见项数', en: 'Item height / visible items' }],
            ['textColor / placeholderColor', '#rrggbb', '#000000 / #999999', { zh: '文字 / 占位色', en: 'Text / placeholder color' }],
            ['menuBackground / hoverBackground', '#rrggbb', '#FFFFFF / #E3F2FD', { zh: '菜单 / 悬停背景', en: 'Menu / hover background' }],
            ['selectedBackground / arrowColor', '#rrggbb', '#E3F2FD / #999999', { zh: '选中背景 / 箭头色', en: 'Selected background / arrow' }],
        ] },
    ],
};

const L = {
    s1: { zh: '下拉选择', en: 'Dropdown select' },
    city: { zh: '城市', en: 'City' },
};

const st = new State({ city: '上海' });

const C1 = `const form = new State({ city: '上海' });

Dropdown({
    placeholder: '请选择城市',
    items: ['北京', '上海', '广州', '深圳'],
    value: ref(form, 'city'),
    width: 220, height: 36,
    onChange: (e) => console.log(e.value, e.index),
});`;
const demo1 = () => [
    Flex({ direction: 'row', alignItems: 'center', gap: 12 }, [
        Text({ text: t(L.city), width: 50, fontSize: 12.5, color: '#475569' }),
        Dropdown({
            id: 'ds-dropdown',
            placeholder: t({ zh: '请选择城市', en: 'Pick a city' }),
            items: ['北京', '上海', '广州', '深圳', '杭州'],
            value: ref(st, 'city'), width: 220, height: 36,
            fontSize: 13, itemHeight: 30, maxVisibleItems: 5,
            borderWidth: 1, borderColor: '#CBD5E1', borderRadius: 6,
        }),
    ]),
    Text({ text: 'selected = ' + st.city, fontSize: 12, color: '#2563EB', margin: [62, 8, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
