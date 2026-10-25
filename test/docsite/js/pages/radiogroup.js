// ── RadioGroup 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/radiogroup.md。
// 单演示小节：RadioGroup 持有 selected，子项只声明 value。
import { View, Flex, Text, RadioButton, RadioGroup, State } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'radiogroup',
    group: 'form',
    title: { zh: 'RadioGroup 单选组', en: 'RadioGroup' },
    desc: {
        zh: '单选按钮的容器管理器：RadioGroup 持有 selected，子项只声明 value，'
            + '组内互斥与选中态由容器统一维护。',
        en: 'Container for radio buttons: RadioGroup owns selected, children only declare value, '
            + 'and the container maintains mutual exclusion and selection.',
    },
    props: [
        { title: { zh: '专有属性', en: 'Props' }, rows: [
            ['name', 'string', '-', { zh: '组名（匹配子项 group）', en: 'Group name (matches child group)' }],
            ['selected', 'string', '-', { zh: '当前选中项的值', en: 'Selected value' }],
            ['onChange', '(e) => {}', '-', { zh: '回调 e={value}', en: 'Callback e={value}' }],
        ] },
    ],
};

const L = {
    s1: { zh: '容器管理选中', en: 'Container-managed selection' },
};

const st = new State({ plan: 'standard' });

const C1 = `const form = new State({ plan: 'standard' });

RadioGroup({
    name: 'plan',
    selected: form.plan,
    onChange: (e) => { form.plan = e.value; },
}, [
    RadioButton({ text: '标准版', value: 'standard' }),
    RadioButton({ text: '旗舰版', value: 'flagship' }),
]);`;
const demo1 = () => [
    RadioGroup({
        name: 'ds-rg-plan', selected: st.plan,
        onChange: (e) => { st.plan = e.value; },
    }, [
        Flex({ direction: 'row', gap: 22 }, [
            RadioButton({ text: t({ zh: '标准版', en: 'Standard' }), value: 'standard' }),
            RadioButton({ text: t({ zh: '旗舰版', en: 'Flagship' }), value: 'flagship' }),
            RadioButton({ text: t({ zh: '企业版', en: 'Enterprise' }), value: 'enterprise' }),
        ]),
    ]),
    Text({ text: 'selected = ' + st.plan, fontSize: 12, color: '#2563EB', margin: [0, 10, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
