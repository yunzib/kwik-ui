// ── Input 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/input.md。
// 单演示小节：姓名 / 密码 / 数字三种形态 + setProp 写入。
import { View, Flex, Text, Button, Input, State, ref, setProp } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'input',
    group: 'form',
    title: { zh: 'Input 单行输入', en: 'Input' },
    desc: {
        zh: '单行文本输入框，支持中文 IME、密码遮罩、数字模式、只读、'
            + '最大长度、光标与聚焦边框；通过 ref 做双向绑定，失焦时同步 State。',
        en: 'Single-line text input with IME, password masking, numeric mode, read-only, '
            + 'max length and focus styling; bind with ref and sync State on blur.',
    },
    props: [
        { title: { zh: '内容', en: 'Content' }, rows: [
            ['value', 'string | ref(state,key)', "''", { zh: '文本值 / 双向绑定', en: 'Text value / two-way binding' }],
            ['placeholder', 'string', "''", { zh: '占位文本', en: 'Placeholder' }],
            ['type', 'text | password | number', 'text', { zh: '输入类型（password 优先）', en: 'Input type (password wins)' }],
            ['readOnly', 'boolean', 'false', { zh: '只读', en: 'Read-only' }],
            ['maxLength', 'number', '0', { zh: '最大字符数（0=不限）', en: 'Max characters (0 = unlimited)' }],
        ] },
        { title: { zh: '数字模式', en: 'Number mode' }, rows: [
            ['min / max', 'number', '-', { zh: '提交时 clamp 到范围', en: 'Clamp on commit' }],
            ['step', 'number', '-', { zh: '提交时按步进对齐', en: 'Snap to step on commit' }],
        ] },
        { title: { zh: '外观与事件', en: 'Style & Events' }, rows: [
            ['fontSize', 'number', '16', { zh: '字号 px', en: 'Font size' }],
            ['textColor', '#rrggbb', '#000000', { zh: '文字颜色', en: 'Text color' }],
            ['placeholderColor', '#rrggbb', '#999999', { zh: '占位符颜色', en: 'Placeholder color' }],
            ['cursorColor', '#rrggbb', '#4285F4', { zh: '光标颜色', en: 'Cursor color' }],
            ['focusedBorderColor', '#rrggbb', '#4285F4', { zh: '聚焦边框色', en: 'Focused border color' }],
            ['onChange', '(value) => {}', '-', { zh: '文本变更回调', en: 'Change callback' }],
        ] },
    ],
};

const L = {
    s1: { zh: '三种形态 + 写入', en: 'Three modes + write' },
    name: { zh: '姓名', en: 'Name' },
    pwd: { zh: '密码', en: 'Password' },
    age: { zh: '年龄（数字）', en: 'Age (number)' },
    fill: { zh: '写入 Input', en: 'Write Input' },
};

const st = new State({ name: '张三', age: '18' });

const C1 = `const form = new State({ name: '张三' });

Input({
    id: 'name',
    value: ref(form, 'name'),
    placeholder: '请输入姓名',
    width: 240, height: 36,
});
// 失焦后 State 才同步，输入过程不打断光标
setProp('name', 'value', '李四');`;
const demo1 = () => [
    Flex({ direction: 'row', gap: 12, alignItems: 'center' }, [
        Text({ text: t(L.name), width: 90, fontSize: 12.5, color: '#475569' }),
        Input({
            id: 'ds-input', value: ref(st, 'name'), placeholder: '请输入姓名',
            width: 240, height: 36, fontSize: 13,
            borderWidth: 1, borderColor: '#CBD5E1', focusedBorderColor: '#2563EB',
            cursorColor: '#2563EB', borderRadius: 6,
        }),
    ]),
    Flex({ direction: 'row', gap: 12, alignItems: 'center', margin: [10, 0, 0, 0] }, [
        Text({ text: t(L.pwd), width: 90, fontSize: 12.5, color: '#475569' }),
        Input({ type: 'password', value: 'kwik-ui', width: 240, height: 36, fontSize: 13,
                borderWidth: 1, borderColor: '#CBD5E1', borderRadius: 6 }),
    ]),
    Flex({ direction: 'row', gap: 12, alignItems: 'center', margin: [10, 0, 0, 0] }, [
        Text({ text: t(L.age), width: 90, fontSize: 12.5, color: '#475569' }),
        Input({ id: 'ds-input-age', type: 'number', min: 0, max: 120, step: 1,
                value: ref(st, 'age'), width: 240, height: 36, fontSize: 13,
                borderWidth: 1, borderColor: '#CBD5E1', borderRadius: 6 }),
        Button({ text: t(L.fill), width: 110, height: 32, fontSize: 12,
                 background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
                 onClick: () => setProp('ds-input', 'value', '李四') }),
    ]),
    Text({ text: '当前 name: ' + st.name, fontSize: 12, color: '#2563EB', margin: [12, 0, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
