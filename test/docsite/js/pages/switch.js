// ── Switch 文档页 ──
// 页契约同 view / textarea：meta + content。手稿 = docs/components/switch.md。
// 四个示例小节：开关列表与联动 / 外观定制 / 主题 token / 命令式读写。
import { View, Flex, Text, Button, Switch, State, ref, getProp, setProp, ThemeProvider, theme } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'switch',
    group: 'form',
    title: { zh: 'Switch 开关', en: 'Switch' },
    desc: {
        zh: '胶囊轨道 + 圆形滑块（带投影）的二元开关：点击切换 checked，经 ref '
            + '双向绑定即时写回 State，onChange 同步回调（e={checked}）；'
            + '无内置标签，常与外部 Text 组合；轨道/滑块配色支持 @token 主题引用。',
        en: 'Pill-track toggle with a shadowed round thumb: tap toggles checked, '
            + 'ref binding writes State instantly, onChange fires with e={checked}. '
            + 'No built-in label — compose with Text; colors accept @theme tokens.',
    },
    props: [
        { title: { zh: '状态与事件', en: 'State & Events' }, rows: [
            ['checked', 'boolean | ref(state,key)', 'false', { zh: '开启状态 / 双向绑定（点击即写回 State）', en: 'On state / two-way binding (writes State on tap)' }],
            ['onChange', '(e) => {}', '-', { zh: '变更回调 e={checked}', en: 'Change callback e={checked}' }],
        ] },
        { title: { zh: '外观', en: 'Appearance' }, rows: [
            ['checkedColor', '#rrggbb | @token', '#1976D2', { zh: '开启轨道色', en: 'On track color' }],
            ['uncheckedColor', '#rrggbb | @token', '#E0E0E0', { zh: '关闭轨道色', en: 'Off track color' }],
            ['thumbColor', '#rrggbb | @token', '#FFFFFF', { zh: '滑块颜色（自带投影）', en: 'Thumb color (shadowed)' }],
            ['trackHeight', 'number', '24', { zh: '轨道高度 px（默认自然宽 48）', en: 'Track height (natural width 48)' }],
            ['thumbSize', 'number', '20', { zh: '滑块直径 px', en: 'Thumb diameter' }],
        ] },
        { title: { zh: '命令式读写', en: 'Imperative access' }, rows: [
            ['getProp(id, "checked")', '"true" | "false"', '-', { zh: '读取开启状态', en: 'Read checked' }],
            ['setProp(id, "checked", v)', '-', '-', { zh: '写入开启状态（接受 true/false/"1"/"0"）', en: 'Write checked (true/false/"1"/"0")' }],
        ] },
    ],
};

const L = {
    s1: { zh: '开关列表与文字联动', en: 'Switch list & labels' },
    s2: { zh: '外观定制', en: 'Styling' },
    s3: { zh: '主题 token', en: 'Theme tokens' },
    s4: { zh: '命令式读写（非受控）', en: 'Imperative access (uncontrolled)' },
    notify: { zh: '消息通知', en: 'Notifications' },
    bluetooth: { zh: '蓝牙', en: 'Bluetooth' },
    wifi: { zh: 'Wi-Fi', en: 'Wi-Fi' },
    on: { zh: '已开启', en: 'On' },
    off: { zh: '已关闭', en: 'Off' },
    flip: { zh: '翻转', en: 'Toggle' },
    read: { zh: 'getProp 读取', en: 'getProp read' },
    hint1: { zh: '点击开关：ref 即时写回 State（右侧文字随之刷新），onChange 同步携带 e.checked。'
            + 'Switch 无内置标签，标签由外部 Text 组合', en: 'Tap: ref writes State instantly (label refreshes), onChange carries e.checked. No built-in label — compose with Text' },
    hint2: { zh: '左起：默认 Material 蓝 / 绿色 / 紫色；下行大小两档——trackHeight 20·thumb 14 与 32·thumb 24', en: 'Left to right: default blue / green / violet; below: two sizes — 20·14 and 32·24' },
    hint3: { zh: 'ThemeProvider 子树内轨道/滑块颜色用 @primary / @surfaceVariant 引用主题；换主题即换配色', en: 'Inside ThemeProvider, track/thumb colors reference @primary / @surfaceVariant; swap theme to recolor' },
    hint4: { zh: '非受控用法：不接 ref，状态由组件自持；外部用 setProp 翻转 / getProp 读取（此时不写 State）', en: 'Uncontrolled: no ref, the widget owns state; flip with setProp / read with getProp (State untouched)' },
};

// ── 各节 State ──
const st1 = new State({ notify: true, bt: false, wifi: true });
const st4 = new State({ read: '-' });

const btn = (text, onClick, w = 84) => Button({
    text, width: w, height: 30, fontSize: 12,
    background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6, onClick,
});
const stateText = (on) => Text({
    text: on ? t(L.on) : t(L.off), fontSize: 12,
    color: on ? '#059669' : '#94A3B8', width: 64,
});

const C1 = `import { Switch, Text, Flex, State, ref } from 'kwikui';

const st = new State({ notify: true, bt: false });

// 每行 = Switch + 外部 Text 标签 + 状态文字；ref 点击即写回 State
const row = (label, key) => Flex({ direction: 'row', alignItems: 'center', gap: 12 }, [
    Switch({ checked: ref(st, key), trackHeight: 28, thumbSize: 20 }),
    Text({ text: label, fontSize: 13 }),
    Text({ text: st[key] ? '已开启' : '已关闭', fontSize: 12 }),
]);

row('消息通知', 'notify');
row('蓝牙', 'bt');

// onChange 单独/并用均可：e = { checked: bool }
Switch({ checked: false, onChange: (e) => console.log(e.checked) });`;

const demo1 = () => [
    Flex({ direction: 'column', gap: 12 }, [
        Flex({ direction: 'row', alignItems: 'center', gap: 12 }, [
            Switch({ id: 'ds-sw-notify', checked: ref(st1, 'notify'),
                     checkedColor: '#2563EB', uncheckedColor: '#CBD5E1', trackHeight: 28, thumbSize: 20 }),
            Text({ text: t(L.notify), fontSize: 13, color: '#334155' }),
            stateText(st1.notify),
        ]),
        Flex({ direction: 'row', alignItems: 'center', gap: 12 }, [
            Switch({ id: 'ds-sw-bt', checked: ref(st1, 'bt'),
                     checkedColor: '#2563EB', uncheckedColor: '#CBD5E1', trackHeight: 28, thumbSize: 20 }),
            Text({ text: t(L.bluetooth), fontSize: 13, color: '#334155' }),
            stateText(st1.bt),
        ]),
        Flex({ direction: 'row', alignItems: 'center', gap: 12 }, [
            Switch({ id: 'ds-sw-wifi', checked: ref(st1, 'wifi'),
                     checkedColor: '#2563EB', uncheckedColor: '#CBD5E1', trackHeight: 28, thumbSize: 20 }),
            Text({ text: t(L.wifi), fontSize: 13, color: '#334155' }),
            stateText(st1.wifi),
        ]),
    ]),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C2 = `import { Switch } from 'kwikui';

// 配色：默认 Material 蓝 / 绿色 / 紫色
Switch({ checked: true });
Switch({ checked: true, checkedColor: '#059669' });
Switch({ checked: true, checkedColor: '#7C3AED' });

// 尺寸：trackHeight 与 thumbSize 自由搭配
Switch({ checked: true, trackHeight: 20, thumbSize: 14 });   // 小
Switch({ checked: true, trackHeight: 32, thumbSize: 24 });   // 大`;

const demo2 = () => [
    Flex({ direction: 'row', gap: 22, alignItems: 'center' }, [
        Switch({ checked: true, checkedColor: '#1976D2', uncheckedColor: '#E0E0E0', trackHeight: 28, thumbSize: 20 }),
        Switch({ checked: true, checkedColor: '#059669', uncheckedColor: '#CBD5E1', trackHeight: 28, thumbSize: 20 }),
        Switch({ checked: true, checkedColor: '#7C3AED', uncheckedColor: '#CBD5E1', trackHeight: 28, thumbSize: 20 }),
    ]),
    Flex({ direction: 'row', gap: 22, alignItems: 'center', margin: [14, 0, 0, 0] }, [
        Switch({ checked: true, checkedColor: '#2563EB', uncheckedColor: '#CBD5E1', trackHeight: 20, thumbSize: 14 }),
        Switch({ checked: true, checkedColor: '#2563EB', uncheckedColor: '#CBD5E1', trackHeight: 32, thumbSize: 24 }),
    ]),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', margin: [10, 0, 0, 0] }),
];

// 自定义主题：primary 用祖母绿，验证 @token 跟随主题
const emerald = theme({
    colors: { primary: '#059669', onPrimary: '#FFFFFF',
              surfaceVariant: '#D1FAE5', onSurface: '#064E3B' },
    shape: { borderRadius: 10 },
});

const C3 = `import { Switch, Text, ThemeProvider, theme, View } from 'kwikui';

const emerald = theme({
    colors: { primary: '#059669', surfaceVariant: '#D1FAE5' },
});

// 子树内轨道颜色引用 @token：换 theme 对象即整体换配色
ThemeProvider({ theme: emerald },
    View({ padding: 12 }, [
        Switch({ checked: true,
                 checkedColor: '@primary',       // 开启轨道
                 uncheckedColor: '@surfaceVariant' }),   // 关闭轨道
    ]));`;

const demo3 = () => [
    ThemeProvider({ theme: emerald },
        View({ padding: [12, 14, 12, 14], background: '#F8FAFC', borderRadius: 8 }, [
            Flex({ direction: 'row', gap: 22, alignItems: 'center' }, [
                Switch({ checked: true, checkedColor: '@primary', uncheckedColor: '@surfaceVariant',
                         trackHeight: 28, thumbSize: 20 }),
                Switch({ checked: false, checkedColor: '@primary', uncheckedColor: '@surfaceVariant',
                         trackHeight: 28, thumbSize: 20 }),
                Text({ text: '@primary / @surfaceVariant', fontSize: 11.5, color: '#64748B' }),
            ]),
        ])),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C4 = `import { Switch, Button, Text, State, setProp, getProp } from 'kwikui';

const st = new State({ read: '-' });

// 非受控：不接 ref，组件自持状态
Switch({ id: 'flag', checked: false, trackHeight: 28, thumbSize: 20 });

// 外部翻转（不经过 State）
Button({ text: '翻转',
        onClick: () => setProp('flag', 'checked',
                               getProp('flag', 'checked') !== 'true') });

// 随时读取
Button({ text: '读取',
        onClick: () => { st.read = getProp('flag', 'checked'); } });`;

const demo4 = () => [
    Flex({ direction: 'row', gap: 14, alignItems: 'center' }, [
        Switch({ id: 'ds-sw-plain', checked: false,
                 checkedColor: '#2563EB', uncheckedColor: '#CBD5E1', trackHeight: 28, thumbSize: 20 }),
        btn(t(L.flip), () => setProp('ds-sw-plain', 'checked',
                                     getProp('ds-sw-plain', 'checked') !== 'true')),
        btn(t(L.read), () => { st4.read = getProp('ds-sw-plain', 'checked'); }, 110),
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
