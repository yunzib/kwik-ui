// ── SpinBox 文档页 ──
// 页契约同 view / textarea：meta + content。手稿 = docs/components/spinbox.md。
// 四个示例小节：绑定与输入 / 约束与只读 / 外观定制 / onChange 与命令式读写。
import { View, Flex, Text, Button, SpinBox, State, ref, getProp, setProp } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'spinbox',
    group: 'form',
    title: { zh: 'SpinBox 步进器', en: 'SpinBox' },
    desc: {
        zh: '数字输入与右侧上下箭头的组合（内部复用 Input 数字模式）：箭头按 step 步进、'
            + '可直接键入浮点（白名单过滤 + 失焦/回车 clamp 与 step 对齐）、'
            + 'ref 双向绑定逐键回写，onChange 载荷是 number。',
        en: 'Numeric field plus stepper arrows (reuses Input number mode inside): '
            + 'arrows step by step, direct float typing (whitelist filter + clamp/step '
            + 'snap on blur/enter), ref binding writes per keystroke, onChange carries a number.',
    },
    props: [
        { title: { zh: '值与约束', en: 'Value & Constraints' }, rows: [
            ['value', 'number | ref(state,key)', '0', { zh: '当前值 / 双向绑定（逐键回写 State）', en: 'Value / two-way binding (writes State per keystroke)' }],
            ['min / max', 'number', '-', { zh: '范围（可选；箭头与提交均 clamp）', en: 'Bounds (optional; clamps arrows & commit)' }],
            ['step', 'number', '1', { zh: '箭头步进 / 提交对齐基准（<=0 按 1）', en: 'Stepper step / commit snap base (<=0 = 1)' }],
            ['readOnly', 'boolean', 'false', { zh: '只读（禁编辑 + 禁箭头）', en: 'Read-only (disables typing & arrows)' }],
        ] },
        { title: { zh: '外观', en: 'Appearance' }, rows: [
            ['arrowsWidth', 'number', '24', { zh: '右侧箭头区宽度 px', en: 'Arrow area width' }],
            ['arrowColor / arrowHoverColor', '#rrggbb', '#999999 / #333333', { zh: '箭头 / 悬停色（上下半区各自高亮）', en: 'Arrow / hover color (per half)' }],
            ['placeholder / fontSize', 'string / number', '- / 16', { zh: '占位符 / 字号', en: 'Placeholder / font size' }],
            ['textColor / placeholderColor', '#rrggbb', '#000000 / #999999', { zh: '文字 / 占位色', en: 'Text / placeholder color' }],
            ['cursorColor / focusedBorderColor', '#rrggbb', '#4285F4 / #4285F4', { zh: '光标 / 聚焦边框色', en: 'Cursor / focused border' }],
        ] },
        { title: { zh: '事件', en: 'Events' }, rows: [
            ['onChange', '(v) => {}', '-', { zh: '值变更回调 v={value:number}（键入实时解析 + 箭头步进均触发；中间态 "-"、"1e" 不外发）', en: 'Change callback v={value:number} (live typing & arrows; mid-states like "-" / "1e" suppressed)' }],
        ] },
        { title: { zh: '命令式读写', en: 'Imperative access' }, rows: [
            ['getProp(id, "value")', 'string', '-', { zh: '读取当前值（最短数字文本）', en: 'Read value (shortest numeric text)' }],
            ['setProp(id, "value", v)', '-', '-', { zh: '写入值（clamp 后同步输入框文本）', en: 'Write value (clamped, syncs field text)' }],
            ['setProp(id, "min" | "max" | "step", v)', '-', '-', { zh: '运行期改约束（同样可写）', en: 'Change bounds at runtime' }],
        ] },
    ],
};

const L = {
    s1: { zh: '绑定与三种输入', en: 'Binding & three inputs' },
    s2: { zh: '约束与只读', en: 'Constraints & read-only' },
    s3: { zh: '外观定制', en: 'Styling' },
    s4: { zh: 'onChange 实时回显与命令式读写', en: 'Live onChange & imperative access' },
    qty: { zh: '数量', en: 'Quantity' },
    price: { zh: '价格 0~10 步进 0.5', en: 'Price 0–10 step 0.5' },
    readonly: { zh: '只读数量', en: 'Read-only qty' },
    live: { zh: '实时回显', en: 'Live echo' },
    set: { zh: '设为 7.5', en: 'Set 7.5' },
    read: { zh: 'getProp 读取', en: 'getProp read' },
    hint1: { zh: '三种输入：点右侧箭头（上▲增下▼减，hover 各自高亮）按 step 步进；直接键入数字'
            + '（浮点可用）；失焦/回车提交时 clamp 到范围并对齐 step。ref 绑定逐键回写 State。',
             en: 'Three inputs: arrows step by step (hover highlights each half); direct float typing; blur/enter commits with clamp & step snap. ref writes State per keystroke.' },
    hint2: { zh: '价格杆演示浮点步进 0.5（键入 9.9 失焦 → clamp 到 10 并对齐 0.5 倍数）；'
            + 'readOnly：输入与箭头均禁用，文本可选中复制',
             en: 'The price box steps 0.5 (type 9.9 then blur → clamps to 10, snapped to 0.5 multiples); readOnly disables typing & arrows, text stays selectable' },
    hint3: { zh: '左：绿色箭头主题（arrowColor/arrowHoverColor + 聚焦边框同色）；右：加宽箭头区 32 + 淡占位', en: 'Left: green arrow theme with matching focus border; right: wider arrow area (32) + light placeholder' },
    hint4: { zh: '上：onChange 实时回显（载荷是 number，v.value 直接参与运算）；下：非受控 + setProp 写值'
            + '（clamp 后同步输入框），min/max/step 运行期同样可写',
             en: 'Top: onChange echoes live (payload is a number — use v.value directly); bottom: uncontrolled + setProp writes (clamped, field synced); min/max/step also settable at runtime' },
};

// ── 各节 State ──
const st1 = new State({ qty: 5 });
const st2 = new State({ price: 4.5 });
const st4 = new State({ live: '0', read: '-' });

const btn = (text, onClick, w = 90) => Button({
    text, width: w, height: 30, fontSize: 12,
    background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6, onClick,
});
const boxBorder = { borderWidth: 1, borderColor: '#CBD5E1', borderRadius: 6 };

const C1 = `import { SpinBox, Text, Flex, State, ref } from 'kwikui';

const form = new State({ qty: 5 });

// ref 双向绑定：箭头/键入均逐键回写 State
Flex({ direction: 'row', alignItems: 'center', gap: 14 }, [
    Text({ text: '数量' }),
    SpinBox({ value: ref(form, 'qty'), min: 0, max: 100, step: 5,
              width: 150, height: 36 }),
    Text({ text: String(form.qty) }),
]);

// 三种输入：箭头按 step 步进 / 直接键入（浮点可用）/
// 失焦·回车提交 → clamp + step 对齐（Input 数字模式内建）`;

const demo1 = () => [
    Flex({ direction: 'row', alignItems: 'center', gap: 14 }, [
        Text({ text: t(L.qty), width: 40, fontSize: 12.5, color: '#475569' }),
        SpinBox({
            id: 'ds-spinbox', value: ref(st1, 'qty'),
            min: 0, max: 100, step: 5, width: 150, height: 36,
            fontSize: 13, cursorColor: '#2563EB', focusedBorderColor: '#2563EB',
            ...boxBorder,
        }),
        Text({ text: 'State.qty ⇒ ' + st1.qty, fontSize: 12, color: '#2563EB' }),
    ]),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C2 = `import { SpinBox, Text, Flex, State, ref } from 'kwikui';

const form = new State({ price: 4.5 });

// 浮点步进 0.5：键入 9.9 失焦 → clamp 到 10 并对齐 0.5 倍数
SpinBox({ value: ref(form, 'price'), min: 0, max: 10, step: 0.5,
        width: 150, height: 36 });

// readOnly：输入与箭头均禁用（文本可选中复制）
SpinBox({ value: 12, readOnly: true, width: 150, height: 36 });`;

const demo2 = () => [
    Flex({ direction: 'row', gap: 26, alignItems: 'center' }, [
        Flex({ direction: 'column', gap: 4 }, [
            Flex({ direction: 'row', alignItems: 'center', gap: 12 }, [
                Text({ text: t(L.price), width: 130, fontSize: 12.5, color: '#475569' }),
                SpinBox({
                    value: ref(st2, 'price'), min: 0, max: 10, step: 0.5,
                    width: 130, height: 36, fontSize: 13,
                    cursorColor: '#059669', focusedBorderColor: '#059669', ...boxBorder,
                }),
                Text({ text: '⇒ ' + st2.price, width: 44, fontSize: 12, color: '#059669' }),
            ]),
        ]),
        Flex({ direction: 'row', alignItems: 'center', gap: 12 }, [
            Text({ text: t(L.readonly), width: 76, fontSize: 12.5, color: '#475569' }),
            SpinBox({
                value: 12, readOnly: true, width: 130, height: 36, fontSize: 13,
                textColor: '#64748B', background: '#F1F5F9', ...boxBorder,
            }),
        ]),
    ]),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C3 = `import { SpinBox } from 'kwikui';

// 绿色箭头主题：arrowColor / arrowHoverColor + 聚焦边框同色
SpinBox({ value: 3, min: 0, step: 1,
        arrowColor: '#059669', arrowHoverColor: '#047857',
        focusedBorderColor: '#059669', width: 150, height: 36 });

// 加宽箭头区 32 + 淡占位
SpinBox({ placeholder: '输入数量…', arrowsWidth: 32,
        placeholderColor: '#CBD5E1', width: 150, height: 36 });`;

const demo3 = () => [
    Flex({ direction: 'row', gap: 24, alignItems: 'center' }, [
        SpinBox({
            value: 3, min: 0, step: 1, width: 150, height: 36, fontSize: 13,
            arrowColor: '#059669', arrowHoverColor: '#047857',
            cursorColor: '#059669', focusedBorderColor: '#059669', ...boxBorder,
        }),
        SpinBox({
            placeholder: t({ zh: '输入数量…', en: 'Enter qty…' }),
            arrowsWidth: 32, width: 150, height: 36, fontSize: 13,
            placeholderColor: '#CBD5E1', ...boxBorder,
        }),
    ]),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', margin: [10, 0, 0, 0] }),
];

const C4 = `import { SpinBox, Button, Text, State, setProp, getProp } from 'kwikui';

const st = new State({ live: '0', read: '-' });

// onChange 实时回调：载荷是 number（v.value 直接参与运算）
SpinBox({ id: 'live', min: 0, max: 100, width: 150, height: 36,
        onChange: (v) => { st.live = String(v.value * 2); } });   // ×2 直算
Text({ text: '×2 ⇒ ' + st.live });

// 非受控 + 命令式读写：setProp 写 value（clamp 后同步输入框）
SpinBox({ id: 'imp', min: 0, max: 100, step: 5, width: 150, height: 36 });
Button({ text: '设为 7.5', onClick: () => setProp('imp', 'value', 7.5) });
Button({ text: '读取', onClick: () => { st.read = getProp('imp', 'value'); } });

// min / max / step 运行期同样可写：
setProp('imp', 'max', 50);`;

const demo4 = () => [
    Flex({ direction: 'row', alignItems: 'center', gap: 12 }, [
        Text({ text: t(L.live), width: 66, fontSize: 12.5, color: '#475569' }),
        SpinBox({
            min: 0, max: 100, step: 1, width: 150, height: 36, fontSize: 13,
            cursorColor: '#2563EB', focusedBorderColor: '#2563EB', ...boxBorder,
            onChange: (v) => { st4.live = String(v.value * 2); },
        }),
        Text({ text: '×2 ⇒ ' + st4.live, fontSize: 12.5, color: '#059669' }),
    ]),
    Flex({ direction: 'row', alignItems: 'center', gap: 10, margin: [14, 0, 0, 0] }, [
        SpinBox({
            id: 'ds-spinbox-imp', min: 0, max: 100, step: 5, width: 150, height: 36,
            fontSize: 13, ...boxBorder,
        }),
        btn(t(L.set), () => setProp('ds-spinbox-imp', 'value', 7.5)),
        btn(t(L.read), () => { st4.read = getProp('ds-spinbox-imp', 'value'); }, 104),
    ]),
    Text({ text: 'getProp ⇒ ' + st4.read, fontSize: 12, color: '#2563EB', margin: [8, 0, 0, 0] }),
    Text({ text: t(L.hint4), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
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
