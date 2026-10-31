// ── Dropdown 文档页 ──
// 页契约同 view / textarea：meta + content。手稿 = docs/components/dropdown.md。
// 五个示例小节：ref 绑定 / 长列表与菜单行为 / 外观定制 / 主题 token / 命令式读写。
import { View, Flex, Text, Button, Dropdown, State, ref, getProp, setProp, ThemeProvider, theme } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'dropdown',
    group: 'form',
    title: { zh: 'Dropdown 下拉选择', en: 'Dropdown' },
    desc: {
        zh: '点击触发区展开菜单：菜单是独立浮层不被祖先裁剪、点外部/触发区/ESC 自动收起，'
            + '悬停高亮、选中高亮、超长列表菜单内滚动；视口底部放不下时自动翻到触发区上方。'
            + 'value 可 ref 双向绑定（初始按 items 匹配选中），onChange 回调 e={value,index}。',
        en: 'Tap to open a menu as an independent layer (never clipped by ancestors); '
            + 'closes on outside tap / trigger tap / ESC. Hover & selection highlight, '
            + 'in-menu scrolling for long lists, auto-flip above the trigger when the '
            + 'viewport bottom is tight. value binds two-way via ref (initial match in items); '
            + 'onChange fires e={value,index}.',
    },
    props: [
        { title: { zh: '数据与值', en: 'Data & Value' }, rows: [
            ['items', 'string[]', '[]', { zh: '选项列表', en: 'Options' }],
            ['value', 'string | ref(state,key)', "''", { zh: '当前值 / 双向绑定（选中即写回 State；按 items 匹配选中）', en: 'Value / two-way binding (writes State on select; matched in items)' }],
            ['selectedIndex', 'number', '-1', { zh: '选中索引（-1=未选；与 value 同给时索引优先）', en: 'Selected index (-1 = none; wins over value)' }],
            ['placeholder', 'string', '请选择...', { zh: '未选择时占位', en: 'Placeholder when empty' }],
        ] },
        { title: { zh: '事件', en: 'Events' }, rows: [
            ['onChange', '(e) => {}', '-', { zh: '选中回调 e={value,index}（选择提交时触发一次）', en: 'Select callback e={value,index} (once per commit)' }],
        ] },
        { title: { zh: '外观与菜单', en: 'Style & Menu' }, rows: [
            ['fontSize', 'number', '14', { zh: '字号 px', en: 'Font size' }],
            ['itemHeight / maxVisibleItems', 'number', '20 / 5', { zh: '选项高 / 可见项数（超出菜单内滚动 + 滚动条）', en: 'Item height / visible items (in-menu scroll beyond)' }],
            ['textColor / placeholderColor', '#rrggbb | @token', '#000000 / #999999', { zh: '文字 / 占位色', en: 'Text / placeholder color' }],
            ['arrowColor', '#rrggbb | @token', '#999999', { zh: '箭头 ▼ 颜色', en: 'Arrow color' }],
            ['menuBackground / hoverBackground', '#rrggbb | @token', '#FFFFFF / #E3F2FD', { zh: '菜单底 / 悬停高亮', en: 'Menu / hover background' }],
            ['selectedBackground', '#rrggbb | @token', '#E3F2FD', { zh: '选中项背景', en: 'Selected background' }],
        ] },
        { title: { zh: '命令式读写', en: 'Imperative access' }, rows: [
            ['getProp(id, "value") / getProp(id, "index")', 'string / "n"', '-', { zh: '读取当前选中值 / 索引（未选 index="-1"）', en: 'Read selection value / index ("-1" when none)' }],
            ['setProp(id, "value", v)', '-', '-', { zh: '按 items 匹配写入（不匹配忽略）', en: 'Write by matching items (ignored if no match)' }],
            ['setProp(id, "index", n)', '-', '-', { zh: '直写索引（-1 清空到占位符）', en: 'Write index directly (-1 clears to placeholder)' }],
        ] },
    ],
};

const L = {
    s1: { zh: 'ref 双向绑定', en: 'ref binding' },
    s2: { zh: '长列表与菜单行为', en: 'Long list & menu behavior' },
    s3: { zh: '外观定制', en: 'Styling' },
    s4: { zh: '主题 token', en: 'Theme tokens' },
    s5: { zh: '命令式读写（非受控）', en: 'Imperative access (uncontrolled)' },
    city: { zh: '城市', en: 'City' },
    prov: { zh: '省份（12 项）', en: 'Province (12 items)' },
    pickCity: { zh: '选 value 广州', en: 'Pick value 广州' },
    pickIdx: { zh: '选 index 3', en: 'Pick index 3' },
    clear: { zh: '清空 (-1)', en: 'Clear (-1)' },
    read: { zh: 'getProp 读取', en: 'getProp read' },
    hint1: { zh: '点击展开；悬停高亮（hoverBackground）、选中高亮（selectedBackground）；'
            + '点选项提交并收起、点外部/再点触发区/ESC 收起。ref 绑定选中即写回 State。',
             en: 'Tap to open; hover & selection highlight; commit on item tap; close on outside tap / trigger tap / ESC. ref writes State on select.' },
    hint2: { zh: '12 项超出 maxVisibleItems=6：菜单内滚轮滚动（一格一项）+ 右侧滚动条；'
            + '菜单贴近视口底部放不下时自动翻转到触发区上方',
             en: '12 items over maxVisibleItems=6: wheel-scroll inside the menu (one item per notch) with a scrollbar; flips above the trigger when the viewport bottom is tight' },
    hint3: { zh: '左：绿色菜单（hover/selected 高亮分色）；右：紫色菜单 + 加大行高 34 与 14 号字', en: 'Left: green menu (distinct hover/selected); right: violet menu + taller items (34) at 14px' },
    hint4: { zh: 'ThemeProvider 子树内文字/菜单配色用 @onSurface / @surface / @surfaceVariant 引用主题；换主题即换配色', en: 'Inside ThemeProvider, text/menu colors reference @onSurface / @surface / @surfaceVariant; swap theme to recolor' },
    hint5: { zh: '非受控：不接 ref，选中态由组件自持；setProp 写 value 按 items 匹配、写 index 直选（-1 清空回占位符）', en: 'Uncontrolled: no ref, the widget owns selection; setProp value matches in items, setProp index selects directly (-1 clears)' },
};

// ── 各节 State ──
const st1 = new State({ city: '上海' });
const st5 = new State({ read: '-' });

const btn = (text, onClick, w = 96) => Button({
    text, width: w, height: 30, fontSize: 12,
    background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6, onClick,
});
const boxBorder = { borderWidth: 1, borderColor: '#CBD5E1', borderRadius: 6 };
const provinces = ['河北', '山西', '辽宁', '吉林', '江苏', '浙江', '安徽', '福建', '山东', '河南', '湖北', '湖南'];

const C1 = `import { Dropdown, Text, Flex, State, ref } from 'kwikui';

const form = new State({ city: '上海' });

// ref 双向绑定：初始值按 items 匹配选中，选中即写回 State
Dropdown({
    placeholder: '请选择城市',
    items: ['北京', '上海', '广州', '深圳', '杭州'],
    value: ref(form, 'city'),
    width: 220, height: 36,
    onChange: (e) => console.log(e.value, e.index),   // { value, index }
});
Text({ text: 'selected = ' + form.city });`;

const demo1 = () => [
    Flex({ direction: 'row', alignItems: 'center', gap: 12 }, [
        Text({ text: t(L.city), width: 50, fontSize: 12.5, color: '#475569' }),
        Dropdown({
            id: 'ds-dropdown',
            placeholder: t({ zh: '请选择城市', en: 'Pick a city' }),
            items: ['北京', '上海', '广州', '深圳', '杭州'],
            value: ref(st1, 'city'), width: 220, height: 36,
            fontSize: 13, itemHeight: 30, maxVisibleItems: 5,
            ...boxBorder,
        }),
    ]),
    Text({ text: 'State.city ⇒ ' + st1.city, fontSize: 12, color: '#2563EB', margin: [10, 0, 0, 0] }),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
];

const C2 = `import { Dropdown } from 'kwikui';

const provinces = ['河北', '山西', '辽宁', '吉林', '江苏', '浙江',
                   '安徽', '福建', '山东', '河南', '湖北', '湖南'];

// 12 项 > maxVisibleItems=6：菜单内滚轮滚动（一格一项）+ 滚动条
Dropdown({ items: provinces, placeholder: '请选择省份',
        itemHeight: 30, maxVisibleItems: 6, width: 220, height: 36 });

// 菜单贴近视口底部放不下时自动翻转到触发区上方（无需配置）`;

const demo2 = () => [
    Flex({ direction: 'row', alignItems: 'center', gap: 12 }, [
        Text({ text: t(L.prov), width: 100, fontSize: 12.5, color: '#475569' }),
        Dropdown({
            id: 'ds-dropdown-long',
            items: provinces, placeholder: t({ zh: '请选择省份', en: 'Pick a province' }),
            width: 220, height: 36,
            fontSize: 13, itemHeight: 30, maxVisibleItems: 6,
            ...boxBorder,
        }),
    ]),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C3 = `import { Dropdown } from 'kwikui';

// 绿色菜单：悬停高亮与选中高亮分色
Dropdown({ items: ['绿茶', '红茶', '乌龙'], placeholder: '茶类',
        hoverBackground: '#DCFCE7', selectedBackground: '#BBF7D0',
        arrowColor: '#059669', width: 200, height: 36 });

// 紫色菜单 + 加大行高 34 与 14 号字
Dropdown({ items: ['JavaScript', 'C++', 'Rust'], placeholder: '语言',
        menuBackground: '#FAF5FF', hoverBackground: '#F3E8FF',
        selectedBackground: '#EDE9FE', arrowColor: '#7C3AED',
        fontSize: 14, itemHeight: 34, maxVisibleItems: 4, width: 200, height: 38 });`;

const demo3 = () => [
    Flex({ direction: 'row', gap: 24, alignItems: 'center' }, [
        Dropdown({
            items: ['绿茶', '红茶', '乌龙'], placeholder: t({ zh: '茶类', en: 'Tea' }),
            hoverBackground: '#DCFCE7', selectedBackground: '#BBF7D0',
            arrowColor: '#059669', width: 200, height: 36,
            fontSize: 13, itemHeight: 30, ...boxBorder,
        }),
        Dropdown({
            items: ['JavaScript', 'C++', 'Rust'], placeholder: t({ zh: '语言', en: 'Language' }),
            menuBackground: '#FAF5FF', hoverBackground: '#F3E8FF',
            selectedBackground: '#EDE9FE', arrowColor: '#7C3AED',
            fontSize: 14, itemHeight: 34, maxVisibleItems: 4, width: 200, height: 38,
            ...boxBorder,
        }),
    ]),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', margin: [10, 0, 0, 0] }),
];

// 自定义主题：暗色卡片上验证 @token 跟随主题
const dark = theme({
    colors: { primary: '#A5B4FC', onPrimary: '#FFFFFF',
              surface: '#1E293B', onSurface: '#E2E8F0', surfaceVariant: '#334155' },
    shape: { borderRadius: 8 },
});

const C4 = `import { Dropdown, ThemeProvider, theme, View } from 'kwikui';

const dark = theme({
    colors: { surface: '#1E293B', onSurface: '#E2E8F0', surfaceVariant: '#334155' },
});

// 子树内配色引用 @token：换 theme 对象即整体换配色
ThemeProvider({ theme: dark },
    View({ padding: 12 }, [
        Dropdown({ items: ['北京', '上海', '广州'], placeholder: '城市',
                textColor: '@onSurface', placeholderColor: '@surfaceVariant',
                arrowColor: '@surfaceVariant',
                menuBackground: '@surface',
                hoverBackground: '@surfaceVariant',
                selectedBackground: '@surfaceVariant' }),
    ]));`;

const demo4 = () => [
    ThemeProvider({ theme: dark },
        View({ padding: [10, 12, 10, 12], background: '#F8FAFC', borderRadius: 8 }, [
            Dropdown({
                items: ['北京', '上海', '广州'], placeholder: t(L.city),
                textColor: '@onSurface', placeholderColor: '@surfaceVariant',
                arrowColor: '@surfaceVariant', menuBackground: '@surface',
                hoverBackground: '@surfaceVariant', selectedBackground: '@surfaceVariant',
                width: 220, height: 36, fontSize: 13, itemHeight: 30, ...boxBorder,
            }),
        ])),
    Text({ text: t(L.hint4), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C5 = `import { Dropdown, Button, Text, State, setProp, getProp } from 'kwikui';

const st = new State({ read: '-' });

// 非受控：不接 ref，选中态由组件自持
Dropdown({ id: 'pick', items: ['北京', '上海', '广州', '深圳'],
        placeholder: '请选择', width: 220, height: 36 });

// setProp 写 value：按 items 匹配（不匹配忽略）
Button({ text: '选 value 广州', onClick: () => setProp('pick', 'value', '广州') });

// setProp 写 index：直选（-1 清空回占位符）
Button({ text: '选 index 3', onClick: () => setProp('pick', 'index', 3) });
Button({ text: '清空', onClick: () => setProp('pick', 'index', -1) });

// getProp 读取当前选中
getProp('pick', 'value');   // "广州"
getProp('pick', 'index');   // "2"`;

const demo5 = () => [
    Flex({ direction: 'row', alignItems: 'center', gap: 10 }, [
        Dropdown({
            id: 'ds-dropdown-imp',
            items: ['北京', '上海', '广州', '深圳'],
            placeholder: t({ zh: '请选择', en: 'Pick' }),
            width: 200, height: 36, fontSize: 13, itemHeight: 30, ...boxBorder,
        }),
        btn(t(L.pickCity), () => setProp('ds-dropdown-imp', 'value', '广州'), 110),
        btn(t(L.pickIdx), () => setProp('ds-dropdown-imp', 'index', 3)),
        btn(t(L.clear), () => setProp('ds-dropdown-imp', 'index', -1), 84),
        btn(t(L.read), () => {
            st5.read = getProp('ds-dropdown-imp', 'value') + ' @ ' + getProp('ds-dropdown-imp', 'index');
        }, 104),
    ]),
    Text({ text: 'getProp ⇒ ' + st5.read, fontSize: 12, color: '#2563EB', margin: [10, 0, 0, 0] }),
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
