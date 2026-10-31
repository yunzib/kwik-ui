// ── TextArea 文档页 ──
// 页契约同 view / input：meta + content。手稿 = docs/components/textarea.md。
// 五个示例小节：非受控读写 / ref 绑定与 onChange / 外观定制 / 约束与只读 / 键盘与选区。
import { View, Flex, Text, Button, TextArea, State, ref, getProp, setProp } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'textarea',
    group: 'form',
    title: { zh: 'TextArea 多行输入', en: 'TextArea' },
    desc: {
        zh: '多行文本输入：Enter 换行、宽度感知软换行、方向键行间导航、拖选/双击选词/'
            + 'Ctrl+A·X·C·V 选区编辑。value 可 ref 双向绑定（失焦回写 State），'
            + 'onChange 逐次编辑回调；非受控时用 getProp / setProp 读写。',
        en: 'Multi-line text input: Enter newlines, width-aware soft wrap, arrow-key '
            + 'navigation, drag/double-click selection with Ctrl+A/X/C/V editing. '
            + 'value binds two-way via ref (writes State on blur); onChange fires per '
            + 'edit; uncontrolled read/write via getProp/setProp.',
    },
    props: [
        { title: { zh: '内容与编辑', en: 'Content & Editing' }, rows: [
            ['value', 'string | ref(state,key)', "''", { zh: '初始多行文本（含 \\n）/ 双向绑定（失焦回写 State）', en: 'Initial text (may contain \\n) / two-way binding (writes State on blur)' }],
            ['placeholder', 'string', "''", { zh: '占位符', en: 'Placeholder' }],
            ['rows', 'number', '4', { zh: '可见行数（决定默认高度）', en: 'Visible rows (sets default height)' }],
            ['readOnly', 'boolean', 'false', { zh: '只读（可聚焦选区复制，不响应编辑）', en: 'Read-only (focusable & copyable, editing ignored)' }],
            ['maxLength', 'number', '0', { zh: '最大字符数（0=不限）', en: 'Max characters (0 = unlimited)' }],
        ] },
        { title: { zh: '外观', en: 'Appearance' }, rows: [
            ['fontSize', 'number', '16', { zh: '字号 px（行高 = 1.4×字号）', en: 'Font size (line height = 1.4×size)' }],
            ['textColor', '#rrggbb', '#000000', { zh: '文字颜色', en: 'Text color' }],
            ['placeholderColor', '#rrggbb', '#999999', { zh: '占位符颜色', en: 'Placeholder color' }],
            ['cursorColor', '#rrggbb', '#4285F4', { zh: '光标颜色', en: 'Cursor color' }],
            ['focusedBorderColor', '#rrggbb', '#4285F4', { zh: '聚焦边框色', en: 'Focused border color' }],
        ] },
        { title: { zh: '事件', en: 'Events' }, rows: [
            ['onChange', '(value) => {}', '-', { zh: '内容变更回调（逐次编辑触发，载荷为整段文本）', en: 'Change callback (per edit, payload is full text)' }],
            ['onClick', '(e) => {}', '-', { zh: '点击回调（可挂起键盘等 JS 逻辑）', en: 'Tap callback (e.g. open keyboard)' }],
        ] },
        { title: { zh: '命令式读写', en: 'Imperative access' }, rows: [
            ['getProp(id, "value")', 'string', '-', { zh: '读取当前全文（含 \\n）', en: 'Read full text (with \\n)' }],
            ['setProp(id, "value", s)', '-', '-', { zh: '写入全文（光标重置到文末）；fontSize / rows 同样可写', en: 'Write full text (cursor to end); fontSize / rows also settable' }],
        ] },
    ],
};

const L = {
    s1: { zh: '基础与非受控读写', en: 'Basics & uncontrolled access' },
    s2: { zh: 'ref 双向绑定与 onChange', en: 'ref binding & onChange' },
    s3: { zh: '外观定制', en: 'Styling' },
    s4: { zh: '约束与只读', en: 'Constraints & read-only' },
    s5: { zh: '键盘与选区操作', en: 'Keyboard & selection' },
    read: { zh: '读取', en: 'Read' },
    write: { zh: '写入', en: 'Write' },
    clear: { zh: '清空', en: 'Clear' },
    bio: { zh: '个人简介（ref 绑定）', en: 'Bio (ref bound)' },
    live: { zh: '实时字数（onChange）', en: 'Live count (onChange)' },
    limit: { zh: '留言（≤30 字）', en: 'Message (≤30)' },
    readonly: { zh: '只读协议文本', en: 'Read-only terms' },
    hint1: { zh: '上：value 只作初始值，之后组件自持内容；读取/写入走 getProp / setProp。'
            + '写入多行文本用 \\n 换行。', en: 'Top: value is initial only; the widget owns text afterwards. '
            + 'Read/write via getProp / setProp; use \\n for newlines.' },
    hint2: { zh: '上：ref 绑定失焦才回写 State（输入过程不打断光标）；下：onChange 每次编辑即触发（适合字数/校验）', en: 'Top: ref writes State on blur only; bottom: onChange fires on every edit (good for counts/validation)' },
    hint3: { zh: '点击聚焦对比边框与光标颜色；行高 = 1.4 × 字号，rows 决定默认高度', en: 'Click to focus and compare border/cursor colors; line height = 1.4 × font size, rows sets default height' },
    hint4: { zh: '超过 30 字被截断（按 Unicode 字符计）；readOnly 可聚焦、可拖选复制，编辑被忽略', en: 'Over 30 chars truncated (Unicode chars); readOnly is focusable & copyable, editing ignored' },
    hint5: { zh: '在下方输入框试操作：单击定位 / 拖动选区（跨行分段高亮）/ 双击选词 / Ctrl+A 全选 / Ctrl+X 剪切 / Ctrl+C 复制 / Ctrl+V 粘贴', en: 'Try in the box below: click to place cursor, drag to select (per-line segments), double-click a word, Ctrl+A/X/C/V' },
};

// ── 各节 State ──
const st1 = new State({ read: '尚未读取' });
const st2 = new State({ bio: '第一行：初始简介\n第二行：失焦后写回 State', len: '0' });
const st4 = new State({ len: '0' });

const boxBorder = { borderWidth: 1, borderColor: '#CBD5E1', borderRadius: 6 };
const btn = (text, onClick) => Button({
    text, width: 84, height: 30, fontSize: 12,
    background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6, onClick,
});

const C1 = `import { TextArea, Button, Text, State, getProp, setProp } from 'kwikui';

const st = new State({ read: '尚未读取' });

// value 只作初始值；之后组件自持内容（非受控）
TextArea({ id: 'bio', rows: 4, value: '第一行初始内容\\n第二行', width: 420 });

// 读取 → 按钮回显
Button({ text: '读取', onClick: () => { st.read = getProp('bio', 'value'); } });
Text({ text: 'value = ' + st.read });

// 外部写入（多行用 \\n）/ 清空
setProp('bio', 'value', '新的内容\\n第二行');
setProp('bio', 'value', '');`;

const demo1 = () => [
    TextArea({
        id: 'ds-ta-basic', width: 440, rows: 4,
        value: '第一行初始内容\n第二行：输入 Enter 换行',
        placeholder: '在这里输入多行内容…',
        fontSize: 13, padding: 10,
        cursorColor: '#2563EB', focusedBorderColor: '#2563EB', ...boxBorder,
    }),
    Flex({ direction: 'row', gap: 10, alignItems: 'center', margin: [10, 0, 0, 0] }, [
        btn(t(L.read), () => { st1.read = String(getProp('ds-ta-basic', 'value') ?? '尚未读取'); }),
        btn(t(L.write), () => setProp('ds-ta-basic', 'value', 'setProp 写入的内容\n第二行：光标重置到文末')),
        btn(t(L.clear), () => setProp('ds-ta-basic', 'value', '')),
    ]),
    Text({ text: 'value = ' + st1.read, fontSize: 12, color: '#2563EB', width: 440, wordWrap: true,
           margin: [10, 0, 0, 0] }),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
];

const C2 = `import { TextArea, Text, State, ref } from 'kwikui';

const st = new State({
    bio: '第一行：初始简介\\n第二行：失焦后写回 State',
    len: '0',
});

// ref 双向绑定：输入过程不打断光标，失焦才把全文写回 State
TextArea({ value: ref(st, 'bio'), rows: 4, width: 420 });
Text({ text: 'State.bio = ' + st.bio });   // 失焦后刷新

// onChange 逐次编辑触发（载荷 = 整段文本），适合实时字数/校验
TextArea({ rows: 3, width: 420,
        onChange: (v) => { st.len = '' + v.length; } });
Text({ text: st.len + ' 字' });`;

const demo2 = () => [
    Text({ text: t(L.bio), fontSize: 12.5, color: '#475569' }),
    TextArea({
        value: ref(st2, 'bio'), width: 440, rows: 4, fontSize: 13, padding: 10,
        cursorColor: '#2563EB', focusedBorderColor: '#2563EB', ...boxBorder,
    }),
    Text({ text: 'State.bio ⇒ ' + st2.bio, fontSize: 12, color: '#2563EB', width: 440, wordWrap: true,
           margin: [6, 0, 0, 0] }),
    Text({ text: t(L.live), fontSize: 12.5, color: '#475569', margin: [14, 0, 0, 0] }),
    TextArea({
        width: 440, rows: 3, placeholder: '输入即统计…', fontSize: 13, padding: 10,
        cursorColor: '#059669', focusedBorderColor: '#059669', ...boxBorder,
        onChange: (v) => { st2.len = '' + v.length; },
    }),
    Flex({ direction: 'row', gap: 8, alignItems: 'center', margin: [6, 0, 0, 0] }, [
        Text({ text: st2.len + ' 字', fontSize: 12, color: st2.len === '0' ? '#64748B' : '#059669' }),
    ]),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
];

const C3 = `import { TextArea } from 'kwikui';

// 光标色 / 聚焦边框色 / 占位色 / 字号均可独立定制
TextArea({ placeholder: '默认蓝：光标 + 聚焦边框', rows: 3, width: 260 });
TextArea({ placeholder: '绿色主题', cursorColor: '#059669',
        focusedBorderColor: '#059669', rows: 3, width: 260 });
TextArea({ placeholder: '紫色主题', cursorColor: '#7C3AED',
        focusedBorderColor: '#7C3AED', rows: 3, width: 260 });
TextArea({ placeholder: '淡占位 + 16 号字', placeholderColor: '#CBD5E1',
        fontSize: 16, rows: 3, width: 260 });`;

const demo3 = () => [
    Flex({ direction: 'row', gap: 16 }, [
        Flex({ direction: 'column', gap: 12 }, [
            TextArea({ placeholder: '默认蓝：光标 + 聚焦边框', rows: 3, width: 260,
                    fontSize: 13, padding: 8, ...boxBorder }),
            TextArea({ placeholder: '绿色主题', cursorColor: '#059669', focusedBorderColor: '#059669',
                    rows: 3, width: 260, fontSize: 13, padding: 8, ...boxBorder }),
        ]),
        Flex({ direction: 'column', gap: 12 }, [
            TextArea({ placeholder: '紫色主题', cursorColor: '#7C3AED', focusedBorderColor: '#7C3AED',
                    rows: 3, width: 260, fontSize: 13, padding: 8, ...boxBorder }),
            TextArea({ placeholder: '淡占位 + 16 号字', placeholderColor: '#CBD5E1',
                    rows: 3, width: 260, fontSize: 16, padding: 8, ...boxBorder }),
        ]),
    ]),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', margin: [10, 0, 0, 0] }),
];

const C4 = `import { TextArea, Text, State } from 'kwikui';

const st = new State({ len: '0' });

// maxLength 截断（按 Unicode 字符计）+ onChange 实时字数
TextArea({ maxLength: 30, placeholder: '最多 30 字', rows: 3, width: 420,
        onChange: (v) => { st.len = '' + v.length; } });
Text({ text: st.len + ' / 30' });

// readOnly：可聚焦、可拖选复制，编辑被忽略
TextArea({ value: '第一条　服务内容……\\n第二条　免责声明……',
        readOnly: true, rows: 3, width: 420 });`;

const demo4 = () => [
    Text({ text: t(L.limit), fontSize: 12.5, color: '#475569' }),
    TextArea({
        maxLength: 30, placeholder: '最多 30 字，超出截断', width: 440, rows: 3,
        fontSize: 13, padding: 10, cursorColor: '#2563EB', ...boxBorder,
        onChange: (v) => { st4.len = '' + v.length; },
    }),
    Text({ text: st4.len + ' / 30', fontSize: 12,
           color: st4.len === '30' ? '#DC2626' : '#64748B', margin: [6, 0, 0, 0] }),
    Text({ text: t(L.readonly), fontSize: 12.5, color: '#475569', margin: [14, 0, 0, 0] }),
    TextArea({
        value: '第一条　服务内容：本组件由 kwik-ui 渲染。\n第二条　免责声明：本文本只读，可复制不可改。',
        readOnly: true, width: 440, rows: 3, fontSize: 13, padding: 10,
        textColor: '#64748B', background: '#F1F5F9', ...boxBorder,
    }),
    Text({ text: t(L.hint4), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
];

const C5 = `// 选区与键盘交互内建，无需额外 API：
//   单击定位光标（按行/列就近落字形边界）
//   按住拖动 → 跨行拖选（逐行分段高亮，拖动不冒泡滚动）
//   双击 → 选词（间隔 <400ms 且位移 <6px）
//   Ctrl+A 全选 / Ctrl+X 剪选区 / Ctrl+C 复制 / Ctrl+V 光标处粘贴
//   Enter 换行；←→↑↓ 行内/行间移动；Home/End 文首/文尾
//   Backspace / Delete 删除；编辑即折叠选区；光标 ~530ms 闪烁
// 选区全树唯一：与 Text / TextView / Input 互斥

TextArea({ value: '单击定位光标\\n拖动可跨行拖选\\n双击选中一个词',
        rows: 5, width: 440 });`;

const demo5 = () => [
    TextArea({
        value: '单击定位光标\n按住拖动跨行拖选（逐行分段高亮）\n双击选中一个词\nCtrl+A 全选后 Ctrl+X 剪切\nEnter 换行，↑↓ 行间导航',
        rows: 6, width: 440, fontSize: 13, padding: 10,
        cursorColor: '#2563EB', focusedBorderColor: '#2563EB', ...boxBorder,
    }),
    Text({ text: t(L.hint5), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
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
