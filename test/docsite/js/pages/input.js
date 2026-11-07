// ── Input 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/input.md。
// 四个示例小节：基础绑定与写入 / 外观定制 / 输入约束 / onChange 实时回显。
import { View, Flex, Text, Button, Input, State, ref, setProp, focus, blur } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'input',
    group: 'form',
    title: { zh: 'Input 单行输入', en: 'Input' },
    desc: {
        zh: '单行文本输入框，支持中文 IME、密码遮罩、数字模式（白名单过滤 + '
            + '提交 clamp/step 对齐）、只读、最大长度、光标与聚焦边框定制；'
            + 'ref 双向绑定失焦同步，onChange 逐键实时回调。'
            + 'focus/blur(id) 命令面程序化聚焦，Tab 键遍历表单焦序（onFocus/onBlur 同源通报）。',
        en: 'Single-line text input with IME, password masking, numeric mode '
            + '(whitelist filter + clamp/step on commit), read-only, max length, '
            + 'cursor/focus styling; ref binds two-way (sync on blur), '
            + 'onChange fires per keystroke. '
            + 'focus/blur(id) drive programmatic focus; Tab walks the form order.',
    },
    props: [
        { title: { zh: '内容', en: 'Content' }, rows: [
            ['value', 'string | ref(state,key)', "''", { zh: '文本值 / 双向绑定（失焦同步 State）', en: 'Text value / two-way binding (sync on blur)' }],
            ['placeholder', 'string', "''", { zh: '占位文本', en: 'Placeholder' }],
            ['type', 'text | password | number', 'text', { zh: '输入类型（password 优先）', en: 'Input type (password wins)' }],
            ['readOnly', 'boolean', 'false', { zh: '只读（可聚焦选区，不响应编辑）', en: 'Read-only (focusable, editing ignored)' }],
            ['maxLength', 'number', '0', { zh: '最大字符数（0=不限）', en: 'Max characters (0 = unlimited)' }],
        ] },
        { title: { zh: '数字模式', en: 'Number mode' }, rows: [
            ['min / max', 'number', '-', { zh: '提交时 clamp 到范围', en: 'Clamp on commit' }],
            ['step', 'number', '-', { zh: '提交时按步进对齐（以 min 为基准）', en: 'Snap to step on commit (based on min)' }],
        ] },
        { title: { zh: '外观', en: 'Style' }, rows: [
            ['fontSize', 'number', '16', { zh: '字号 px', en: 'Font size' }],
            ['textColor', '#rrggbb', '#000000', { zh: '文字颜色', en: 'Text color' }],
            ['placeholderColor', '#rrggbb', '#999999', { zh: '占位符颜色', en: 'Placeholder color' }],
            ['cursorColor', '#rrggbb', '#4285F4', { zh: '光标颜色', en: 'Cursor color' }],
            ['focusedBorderColor', '#rrggbb', '#4285F4', { zh: '聚焦边框色', en: 'Focused border color' }],
        ] },
        { title: { zh: '事件', en: 'Events' }, rows: [
            ['onChange', '(value) => {}', '-', { zh: '文本变更回调（逐键触发）', en: 'Change callback (per keystroke)' }],
            ['onClick', '(e) => {}', '-', { zh: '点击回调（可挂起键盘等 JS 逻辑）', en: 'Tap callback (e.g. open keyboard)' }],
            ['onFocus', '(e) => {}', '-', { zh: '获得焦点（载荷 e={mode: \'text\'|\'number\'}；点选 / Tab / focus(id) 同源）', en: 'Focus gained (e={mode}; tap / Tab / focus(id))' }],
            ['onBlur', '(e) => {}', '-', { zh: '失去焦点（同上；失焦同步 ref 在此先；Enter 提交的统一失焦同源触发）', en: 'Focus lost (same sources; ref sync precedes; Enter-submit blur included)' }],
            ['onEnter', '(e) => {}', '-', { zh: 'Enter 提交（载荷 e={value} 引擎处理后值；仅回车触发，隐式失焦不发；次序 onChange→onEnter→onBlur；TextArea 仅到 onEnter）', en: 'Enter submitted (e={value}), only on \\\\n key; order change→enter→blur; TextArea stops at enter' }],
        ] },
    ],
};
// 命令面（非组件属性）：focus(id) 聚焦指定组件；blur(id?) 定点失焦 / 缺省失焦当前。
// Tab 键遍历 acceptsFocus 表单焦序，Shift+Tab 后移（C5）。

const L = {
    s1: { zh: '三种形态 + 外部写入', en: 'Three modes + external write' },
    s2: { zh: '外观定制', en: 'Styling' },
    s3: { zh: '输入约束', en: 'Constraints' },
    s4: { zh: 'onChange 实时回显', en: 'Live onChange echo' },
    s5: { zh: '编程聚焦 focus/blur + Tab 遍历', en: 'Programmatic focus + Tab' },
    fA: { zh: '甲（focus 目标）', en: 'Field A (focus target)' },
    fB: { zh: '乙（focus 目标）', en: 'Field B (focus target)' },
    bA: { zh: '聚焦甲', en: 'Focus A' },
    bB: { zh: '聚焦乙', en: 'Focus B' },
    bBlur: { zh: '失焦乙', en: 'Blur B' },
    bBlurAll: { zh: '全失焦', en: 'Blur all' },
    lastBadge: { zh: '最近焦点事件: ', en: 'Last focus event: ' },
    gainA: { zh: '甲 获得焦点', en: 'A focused' },
    loseA: { zh: '甲 失去焦点', en: 'A blurred' },
    gainB: { zh: '乙 获得焦点', en: 'B focused' },
    loseB: { zh: '乙 失去焦点', en: 'B blurred' },
    dash: { zh: '—（点上方按钮或按 Tab 试试）', en: '— (try buttons above or press Tab)' },
    hintFocus: { zh: 'focus(id)/blur(id?) 命令面走 FocusManager 统一通路——onFocus/onBlur 槽'
                     + '与键盘路由同步生效；Tab 键在甲/乙间循环聚焦（Shift+Tab 反向），'
                     + '焦点边框/光标由组件自身渲染反馈。',
                 en: 'focus(id)/blur(id?) route through FocusManager — onFocus/onBlur slots '
                     + 'and keyboard routing stay in sync; Tab cycles between A/B (Shift+Tab reverses); '
                     + 'focus border/cursor come from the component itself.' },
    name: { zh: '姓名', en: 'Name' },
    pwd: { zh: '密码', en: 'Password' },
    age: { zh: '年龄（数字）', en: 'Age (number)' },
    fill: { zh: '写入 Input', en: 'Write Input' },
    readonly: { zh: '只读', en: 'Read-only' },
    nick: { zh: '昵称（≤10 字）', en: 'Nickname (≤10)' },
    price: { zh: '价格（0~120 步进 5）', en: 'Price (0~120 step 5)' },
    live: { zh: '实时输入', en: 'Live input' },
    sync: { zh: 'ref 同步（失焦生效）', en: 'ref sync (on blur)' },
    hintMax: { zh: '输入超 10 字会被截断；字数实时统计见右侧', en: 'Over 10 chars truncated; live count on the right' },
    hintNum: { zh: '输入 999 回车/失焦 → clamp 到 120；任意值对齐 5 的倍数', en: 'Type 999 then blur → clamped to 120; values snap to multiples of 5' },
    hintRo: { zh: 'readOnly：可聚焦可复制，编辑被忽略', en: 'readOnly: focusable & copyable, editing ignored' },
    hintLive: { zh: '上：onChange 逐键触发（实时）；下：ref 绑定失焦才写 State（不打断输入过程）', en: 'Top: onChange fires per keystroke; bottom: ref writes State on blur only' },
};

// ── 各节 State ──
const st = new State({ name: '张三', age: '18' });
const stMax = new State({ nick: '', len: '0' });
const stNum = new State({ price: '45' });
const stLive = new State({ live: '', sync: '失焦同步' });
const stFocus = new State({ badge: '' });

const boxBorder = { borderWidth: 1, borderColor: '#CBD5E1', borderRadius: 6 };

const C1 = `import { Input, Text, Button, Flex, State, ref, setProp } from 'kwikui';

const form = new State({ name: '张三', age: '18' });

// 行 1：ref 双向绑定——失焦写回 State，回显 Text 随之刷新
Input({ id: 'name', value: ref(form, 'name'), placeholder: '请输入姓名',
        width: 240, height: 36 });
// 行 2：密码遮罩
Input({ type: 'password', value: 'kwik-ui', width: 240, height: 36 });
// 行 3：数字模式 + 外部写入按钮
Input({ id: 'age', type: 'number', min: 0, max: 120, step: 1,
        value: ref(form, 'age'), width: 240, height: 36 });
Button({ text: '写入 Input', width: 110, height: 32,
         onClick: () => setProp('name', 'value', '李四') });

Text({ text: '当前 name: ' + form.name });`;

const demo1 = () => [
    Flex({ direction: 'row', gap: 12, alignItems: 'center' }, [
        Text({ text: t(L.name), width: 130, fontSize: 12.5, color: '#475569' }),
        Input({
            id: 'ds-input', value: ref(st, 'name'), placeholder: '请输入姓名',
            width: 240, height: 36, fontSize: 13,
            cursorColor: '#2563EB', focusedBorderColor: '#2563EB', ...boxBorder,
        }),
    ]),
    Flex({ direction: 'row', gap: 12, alignItems: 'center', margin: [10, 0, 0, 0] }, [
        Text({ text: t(L.pwd), width: 130, fontSize: 12.5, color: '#475569' }),
        Input({ type: 'password', value: 'kwik-ui', width: 240, height: 36, fontSize: 13, ...boxBorder }),
    ]),
    Flex({ direction: 'row', gap: 12, alignItems: 'center', margin: [10, 0, 0, 0] }, [
        Text({ text: t(L.age), width: 130, fontSize: 12.5, color: '#475569' }),
        Input({ id: 'ds-input-age', type: 'number', min: 0, max: 120, step: 1,
                value: ref(st, 'age'), width: 240, height: 36, fontSize: 13, ...boxBorder }),
        Button({ text: t(L.fill), width: 110, height: 32, fontSize: 12,
                 background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
                 onClick: () => setProp('ds-input', 'value', '李四') }),
    ]),
    Text({ text: '当前 name: ' + st.name, fontSize: 12, color: '#2563EB', margin: [12, 0, 0, 0] }),
];

const C2 = `import { Input } from 'kwikui';

// 光标色 / 聚焦边框色 / 占位色 / 字号均可独立定制
Input({ placeholder: '默认蓝：光标 + 聚焦边框', width: 260, height: 34 });
Input({ placeholder: '绿色光标', cursorColor: '#059669',
        focusedBorderColor: '#059669', width: 260, height: 34 });
Input({ placeholder: '紫色聚焦边框', focusedBorderColor: '#7C3AED',
        cursorColor: '#7C3AED', width: 260, height: 34 });
Input({ placeholder: '淡占位 + 17 号字', placeholderColor: '#CBD5E1',
        fontSize: 17, width: 260, height: 38 });`;

const demo2 = () => [
    Flex({ direction: 'row', gap: 20 }, [
        Flex({ direction: 'column', gap: 10 }, [
            Input({ placeholder: '默认蓝：光标 + 聚焦边框', width: 260, height: 34, fontSize: 13, ...boxBorder }),
            Input({ placeholder: '绿色光标', cursorColor: '#059669', focusedBorderColor: '#059669',
                    width: 260, height: 34, fontSize: 13, ...boxBorder }),
        ]),
        Flex({ direction: 'column', gap: 10 }, [
            Input({ placeholder: '紫色聚焦边框', focusedBorderColor: '#7C3AED', cursorColor: '#7C3AED',
                    width: 260, height: 34, fontSize: 13, ...boxBorder }),
            Input({ placeholder: '淡占位 + 17 号字', placeholderColor: '#CBD5E1',
                    width: 260, height: 38, fontSize: 17, ...boxBorder }),
        ]),
    ]),
    Text({ text: '点进输入框对比聚焦边框与光标颜色', fontSize: 11, color: '#94A3B8', margin: [10, 0, 0, 0] }),
];

const C3 = `import { Input, Text, Flex, State, ref } from 'kwikui';

const st = new State({ nick: '', len: '0', price: '45' });

// maxLength 截断 + onChange 实时字数
Input({ maxLength: 10, placeholder: '最多 10 字', width: 220, height: 34,
        onChange: (v) => { st.len = '' + v.length; } });
Text({ text: st.len + ' / 10' });

// readOnly：可聚焦/复制，编辑忽略
Input({ value: '只读内容：可复制不可改', readOnly: true, width: 220, height: 34 });

// 数字模式：白名单过滤 + 失焦 clamp/step 对齐 + ref 同步当前值
Input({ type: 'number', min: 0, max: 120, step: 5, placeholder: '0~120',
        value: ref(st, 'price'), width: 220, height: 34 });
Text({ text: '当前: ' + st.price });`;

const demo3 = () => [
    Flex({ direction: 'row', gap: 12, alignItems: 'center' }, [
        Text({ text: t(L.nick), width: 130, fontSize: 12.5, color: '#475569' }),
        Input({ maxLength: 10, placeholder: '最多 10 字', width: 220, height: 34, fontSize: 13,
                cursorColor: '#2563EB', ...boxBorder,
                onChange: (v) => { stMax.len = '' + v.length; } }),
        Text({ text: stMax.len + ' / 10', width: 52, fontSize: 12, color: stMax.len === '10' ? '#DC2626' : '#64748B' }),
    ]),
    Text({ text: t(L.hintMax), fontSize: 11, color: '#94A3B8', margin: [4, 0, 0, 142] }),
    Flex({ direction: 'row', gap: 12, alignItems: 'center', margin: [12, 0, 0, 0] }, [
        Text({ text: t(L.readonly), width: 130, fontSize: 12.5, color: '#475569' }),
        Input({ value: '只读内容：可复制不可改', readOnly: true, width: 220, height: 34, fontSize: 13,
                textColor: '#64748B', background: '#F1F5F9', ...boxBorder }),
    ]),
    Text({ text: t(L.hintRo), fontSize: 11, color: '#94A3B8', margin: [4, 0, 0, 142] }),
    Flex({ direction: 'row', gap: 12, alignItems: 'center', margin: [12, 0, 0, 0] }, [
        Text({ text: t(L.price), width: 130, fontSize: 12.5, color: '#475569' }),
        Input({ type: 'number', min: 0, max: 120, step: 5, value: ref(stNum, 'price'),
                placeholder: '0~120', width: 220, height: 34, fontSize: 13,
                cursorColor: '#2563EB', ...boxBorder }),
        Text({ text: '当前: ' + stNum.price, fontSize: 12, color: '#2563EB' }),
    ]),
    Text({ text: t(L.hintNum), fontSize: 11, color: '#94A3B8', margin: [4, 0, 0, 142] }),
];

const C4 = `import { Input, Text, Flex, State, ref } from 'kwikui';

const st = new State({ live: '', sync: '失焦同步' });

// 上行 onChange 逐键触发：输入即回显（适合实时搜索/校验/字数）
Input({ placeholder: '输入即回显', width: 240, height: 34,
        onChange: (v) => { st.live = v; } });
Text({ text: '⇒ ' + st.live });

// 下行 ref 双向绑定：输入过程不打断光标，失焦才写 State（回显随之刷新）
Input({ value: ref(st, 'sync'), width: 240, height: 34 });
Text({ text: '⇒ ' + st.sync });`;

const demo4 = () => [
    Flex({ direction: 'row', gap: 12, alignItems: 'center' }, [
        Text({ text: t(L.live), width: 130, fontSize: 12.5, color: '#475569' }),
        Input({ placeholder: '输入即回显', width: 240, height: 34, fontSize: 13,
                cursorColor: '#2563EB', ...boxBorder,
                onChange: (v) => { stLive.live = v; } }),
        Text({ text: '⇒ ' + stLive.live, fontSize: 12.5, color: '#059669' }),
    ]),
    Flex({ direction: 'row', gap: 12, alignItems: 'center', margin: [10, 0, 0, 0] }, [
        Text({ text: t(L.sync), width: 130, fontSize: 12.5, color: '#475569' }),
        Input({ value: ref(stLive, 'sync'), width: 240, height: 34, fontSize: 13, ...boxBorder }),
        Text({ text: '⇒ ' + stLive.sync, fontSize: 12.5, color: '#2563EB' }),
    ]),
    Text({ text: t(L.hintLive), fontSize: 11, color: '#94A3B8', margin: [10, 0, 0, 0] }),
];

// ── ⑤ 编程聚焦 focus/blur 命令面 + Tab 遍历（镜像 test/ui input.js 六卡）──
const C5 = `import { Input, State, focus, blur } from 'kwikui';

// 程序化聚焦：与点选同源——onFocus/onBlur 槽与键盘路由同步生效
focus('mail');              // 聚焦指定 id
blur('mail');               // 定点失焦（该 id 正持有焦点才收）
blur();                     // 失焦当前焦点

Input({ id: 'mail', onFocus: () => { st.badge = '聚焦'; },
        onBlur:  () => { st.badge = '失焦'; } });

// Tab 键遍历表单焦序（Input/TextArea/TextView/SpinBox 等可聚焦控件），
// Shift+Tab 后移；焦点边框/光标由组件自身渲染反馈`;

const demo5 = () => [
    Flex({ direction: 'row', gap: 12, alignItems: 'center' }, [
        Text({ text: t(L.fA), width: 130, fontSize: 12.5, color: '#475569' }),
        Input({ id: 'ds-focus-a', placeholder: t(L.fA), width: 220, height: 34, fontSize: 13,
                cursorColor: '#2563EB', ...boxBorder,
                onFocus: () => { stFocus.badge = t(L.gainA); },
                onBlur: () => { stFocus.badge = t(L.loseA); } }),
    ]),
    Flex({ direction: 'row', gap: 12, alignItems: 'center', margin: [10, 0, 0, 0] }, [
        Text({ text: t(L.fB), width: 130, fontSize: 12.5, color: '#475569' }),
        Input({ id: 'ds-focus-b', placeholder: t(L.fB), width: 220, height: 34, fontSize: 13,
                cursorColor: '#2563EB', ...boxBorder,
                onFocus: () => { stFocus.badge = t(L.gainB); },
                onBlur: () => { stFocus.badge = t(L.loseB); } }),
    ]),
    Flex({ direction: 'row', gap: 8, margin: [10, 0, 0, 0] }, [
        Button({ text: t(L.bA), width: 76, height: 32, fontSize: 12, background: '#2563EB',
                 color: '#FFFFFF', borderRadius: 6, onClick: () => focus('ds-focus-a') }),
        Button({ text: t(L.bB), width: 76, height: 32, fontSize: 12, background: '#059669',
                 color: '#FFFFFF', borderRadius: 6, onClick: () => focus('ds-focus-b') }),
        Button({ text: t(L.bBlur), width: 76, height: 32, fontSize: 12, background: '#64748B',
                 color: '#FFFFFF', borderRadius: 6, onClick: () => blur('ds-focus-b') }),
        Button({ text: t(L.bBlurAll), width: 76, height: 32, fontSize: 12, background: '#DC2626',
                 color: '#FFFFFF', borderRadius: 6, onClick: () => blur() }),
    ]),
    Text({ text: t(L.lastBadge) + (stFocus.badge || t(L.dash)), fontSize: 12, color: '#2563EB',
           margin: [10, 0, 0, 0] }),
    Text({ text: t(L.hintFocus), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true,
           margin: [6, 0, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    ...section(t(L.s3), demo3(), C3, innerW),
    ...section(t(L.s4), demo4(), C4, innerW),
    ...section(t(L.s5), demo5(), C5, innerW),
    propsTable(t, meta.props, innerW),
];
