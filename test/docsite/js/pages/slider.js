// ── Slider 文档页 ──
// 页契约同 view / textarea：meta + content。手稿 = docs/components/slider.md。
// 五个示例小节：双向绑定与交互 / 量程步长与竖直 / 外观定制 / 主题 token / onChange 与命令式读写。
import { View, Flex, Text, Button, Slider, State, ref, getProp, setProp, ThemeProvider, theme } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'slider',
    group: 'form',
    title: { zh: 'Slider 滑杆', en: 'Slider' },
    desc: {
        zh: '水平/竖直滑动条：拖拽、点击轨道跳转、方向键步进（水平 ←→ / 竖直 ↑↓）；'
            + 'ref 双向绑定拖拽中持续回写且不打断手势，onChange 实时回调 e={value}；'
            + 'step 步长取整（<=0 连续），showThumb=false 为平板风格，配色支持 @token。',
        en: 'Horizontal/vertical slider: drag, track-tap jump, arrow-key stepping '
            + '(<- -> horizontal / up-down vertical); ref binding writes State continuously '
            + 'without interrupting the drag, onChange fires live with e={value}; '
            + 'step snapping (<=0 = continuous), showThumb=false for flat style, @theme tokens.',
    },
    props: [
        { title: { zh: '值与范围', en: 'Value & Range' }, rows: [
            ['value', 'number | ref(state,key)', '0', { zh: '当前值 / 双向绑定（拖拽中持续写回 State）', en: 'Value / two-way binding (writes State during drag)' }],
            ['min / max', 'number', '0 / 100', { zh: '量程（越界值被 clamp）', en: 'Range (clamped)' }],
            ['step', 'number', '1', { zh: '步长取整（<=0 连续；键盘步进也用它）', en: 'Step snapping (<=0 = continuous; also keyboard delta)' }],
            ['vertical', 'boolean', 'false', { zh: '竖直方向（底部=min 顶部=max）', en: 'Vertical (bottom=min, top=max)' }],
        ] },
        { title: { zh: '外观', en: 'Appearance' }, rows: [
            ['color', '#rrggbb | @token', '#1976D2', { zh: '滑块描边 + 激活轨道色', en: 'Thumb border + active track' }],
            ['trackColor', '#rrggbb | @token', '#E0E0E0', { zh: '背景轨道色', en: 'Track color' }],
            ['thumbColor / thumbBorderColor', '#rrggbb | @token', '#FFFFFF / color', { zh: '滑块填充 / 描边（描边缺省跟随 color）', en: 'Thumb fill / border (defaults to color)' }],
            ['thumbSize / trackHeight', 'number', '20 / 6', { zh: '滑块直径 / 轨道厚', en: 'Thumb size / track thickness' }],
            ['showThumb', 'boolean', 'true', { zh: '显示滑块（false=平板风格仅轨道）', en: 'Show thumb (false = track-only flat style)' }],
        ] },
        { title: { zh: '事件', en: 'Events' }, rows: [
            ['onChange', '(e) => {}', '-', { zh: '值变化回调 e={value}（拖拽/点击/键盘全程持续触发）', en: 'Change callback e={value} (continuous during drag/tap/keys)' }],
        ] },
        { title: { zh: '命令式读写', en: 'Imperative access' }, rows: [
            ['getProp(id, "value")', 'string', '-', { zh: '读取当前值', en: 'Read value' }],
            ['setProp(id, "value", v)', '-', '-', { zh: '写入值（clamp + step 取整后生效）', en: 'Write value (clamped & step-snapped)' }],
        ] },
    ],
};

const L = {
    s1: { zh: '双向绑定与三种交互', en: 'Binding & three interactions' },
    s2: { zh: '量程、步长与竖直方向', en: 'Range, step & vertical' },
    s3: { zh: '外观定制', en: 'Styling' },
    s4: { zh: '主题 token', en: 'Theme tokens' },
    s5: { zh: 'onChange 实时回显与命令式读写', en: 'Live onChange & imperative access' },
    vol: { zh: '音量', en: 'Volume' },
    volFlat: { zh: '音量（平板）', en: 'Volume (flat)' },
    bright: { zh: '亮度', en: 'Brightness' },
    temp: { zh: '温度 16~30 步进 0.5', en: 'Temp 16–30 step 0.5' },
    flat: { zh: '平板风格', en: 'Flat style' },
    live: { zh: '实时回显', en: 'Live echo' },
    set25: { zh: '设为 25', en: 'Set 25' },
    set85: { zh: '设为 85', en: 'Set 85' },
    read: { zh: 'getProp 读取', en: 'getProp read' },
    hint1: { zh: '三种交互：拖动滑块 / 点击轨道直接跳转 / 聚焦后方向键按 step 步进；'
            + 'ref 绑定拖拽全程持续写回 State 且不打断手势', en: 'Three interactions: drag the thumb, tap the track to jump, focus + arrow keys step by step; ref writes State all the way without interrupting the gesture' },
    hint2: { zh: '移动端宽竖直样式：厚轨道（trackHeight 30）+ 大滑块（thumb 28），底部=min 顶部=max，键盘改用 ↑↓；'
            + '右侧温度杆演示非整数步进 0.5（显示一位小数）',
             en: 'Mobile-style wide verticals: thick track (30) + big thumb (28), bottom=min top=max, keys become up/down; the temp slider shows fractional 0.5 stepping' },
    hint3: { zh: '左：绿色主题配色；中：玫红 + 加粗轨道（trackHeight 10·thumb 26）；右：showThumb=false 平板风格（仅激活段，无滑块）。'
            + '滑块自带 Material 投影（内建 y+2·blur 4），宽轨道下观感自然',
             en: 'Left: green palette; middle: rose + thick track (10·26); right: showThumb=false flat style. The thumb carries a built-in Material drop shadow (y+2, blur 4)' },
    hint4: { zh: 'ThemeProvider 子树内 color / trackColor 用 @primary / @surfaceVariant 引用主题；换主题即换配色', en: 'Inside ThemeProvider, color / trackColor reference @primary / @surfaceVariant; swap theme to recolor' },
    hint5: { zh: '上：onChange 全程实时回显（拖到哪显示到哪）；下：非受控滑杆 + setProp 外部设值（clamp + 步进取整生效）', en: 'Top: onChange echoes live throughout the drag; bottom: uncontrolled slider + setProp writes (clamped & step-snapped)' },
};

// ── 各节 State ──
const st1 = new State({ vol: 42 });
const st2 = new State({ vol: 35, bright: 70, temp: 22.5 });
const st5 = new State({ live: '0', read: '-' });

const btn = (text, onClick, w = 90) => Button({
    text, width: w, height: 30, fontSize: 12,
    background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6, onClick,
});

const C1 = `import { Slider, Text, Flex, State, ref } from 'kwikui';

const form = new State({ vol: 42 });

// ref 双向绑定：拖拽全程持续写回 State，不打断手势
Flex({ direction: 'row', alignItems: 'center', gap: 14 }, [
    Text({ text: '音量' }),
    Slider({ value: ref(form, 'vol'), min: 0, max: 100, step: 1,
             width: 300, color: '#2563EB' }),
    Text({ text: String(form.vol) }),
]);

// 三种交互：拖动滑块 / 点击轨道跳转 / 方向键 ←→ 步进`;

const demo1 = () => [
    Flex({ direction: 'row', alignItems: 'center', gap: 14 }, [
        Text({ text: t(L.vol), width: 40, fontSize: 12.5, color: '#475569' }),
        Slider({
            id: 'ds-slider', value: ref(st1, 'vol'),
            min: 0, max: 100, step: 1, width: 300,
            trackHeight: 8, thumbSize: 24,
            color: '#2563EB', trackColor: '#E2E8F0',
        }),
        Text({ text: String(st1.vol), width: 36, fontSize: 12.5, color: '#2563EB' }),
    ]),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C2 = `import { Slider, Text, Flex, State, ref } from 'kwikui';

const form = new State({ vol: 35, bright: 70, temp: 22.5 });

// 移动端宽竖直样式：厚轨道（trackHeight 30）+ 大滑块（thumb 28）
// 竖直方向底部=min 顶部=max，键盘改用 ↑↓
const col = (label, child, val) => Flex({ direction: 'column', alignItems: 'center', gap: 6 }, [
    Text({ text: val }), child, Text({ text: label, fontSize: 11, color: '#94A3B8' }),
]);

col('亮度', Slider({ value: ref(form, 'bright'), vertical: true, height: 150,
        trackHeight: 30, thumbSize: 28, color: '#2563EB' }), form.bright + '%');

// 平板变体：showThumb=false 仅厚轨道激活段（无滑块无投影）
col('音量', Slider({ value: ref(form, 'vol'), vertical: true, height: 150,
        trackHeight: 30, showThumb: false, color: '#059669' }), String(form.vol));

// 非整数步进 0.5 + 自定义量程 16~30（水平）
Slider({ value: ref(form, 'temp'), min: 16, max: 30, step: 0.5, width: 260 });`;

const demo2 = () => [
    Flex({ direction: 'row', gap: 34, alignItems: 'flexEnd' }, [
        Flex({ direction: 'column', alignItems: 'center', gap: 6 }, [
            Text({ text: String(st2.vol) + '%', fontSize: 12, color: '#059669' }),
            Slider({
                value: ref(st2, 'vol'), vertical: true,
                height: 150, trackHeight: 30, showThumb: false,
                color: '#059669', trackColor: '#E2E8F0',
            }),
            Text({ text: t(L.volFlat), fontSize: 11, color: '#94A3B8' }),
        ]),
        Flex({ direction: 'column', alignItems: 'center', gap: 6 }, [
            Text({ text: String(st2.bright) + '%', fontSize: 12, color: '#2563EB' }),
            Slider({
                value: ref(st2, 'bright'), vertical: true,
                height: 150, trackHeight: 30, thumbSize: 28,
                color: '#2563EB', trackColor: '#E2E8F0',
            }),
            Text({ text: t(L.bright), fontSize: 11, color: '#94A3B8' }),
        ]),
        Flex({ direction: 'column', gap: 6, width: 300 }, [
            Flex({ direction: 'row', alignItems: 'center', gap: 12 }, [
                Slider({
                    value: ref(st2, 'temp'), min: 16, max: 30, step: 0.5, width: 220,
                    trackHeight: 8, thumbSize: 24,
                    color: '#D97706', trackColor: '#E2E8F0',
                }),
                Text({ text: String(st2.temp) + '°C', width: 52, fontSize: 12, color: '#D97706' }),
            ]),
            Text({ text: t(L.temp), fontSize: 11, color: '#94A3B8' }),
        ]),
    ]),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C3 = `import { Slider } from 'kwikui';

// 绿色主题配色（color 管滑块描边 + 激活轨道）
Slider({ value: 62, color: '#059669', trackColor: '#D1FAE5', width: 240 });

// 玫红 + 加粗轨道大滑块
Slider({ value: 38, color: '#E11D48', trackColor: '#FFE4E6',
        trackHeight: 10, thumbSize: 26, width: 240 });

// showThumb=false：平板风格（仅激活段，无滑块）
Slider({ value: 80, showThumb: false, trackHeight: 8,
         color: '#7C3AED', trackColor: '#EDE9FE', width: 240 });`;

const demo3 = () => [
    Flex({ direction: 'column', gap: 16 }, [
        Slider({ value: 62, color: '#059669', trackColor: '#D1FAE5', width: 240 }),
        Slider({ value: 38, color: '#E11D48', trackColor: '#FFE4E6',
                 trackHeight: 10, thumbSize: 26, width: 240 }),
        Slider({ value: 80, showThumb: false, trackHeight: 8,
                 color: '#7C3AED', trackColor: '#EDE9FE', width: 240 }),
    ]),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

// 自定义主题：primary 用青色，验证 @token 跟随主题
const cyan = theme({
    colors: { primary: '#0891B2', onPrimary: '#FFFFFF',
              surfaceVariant: '#CFFAFE', onSurface: '#164E63' },
    shape: { borderRadius: 8 },
});

const C4 = `import { Slider, ThemeProvider, theme, View } from 'kwikui';

const cyan = theme({
    colors: { primary: '#0891B2', surfaceVariant: '#CFFAFE' },
});

// 子树内 color / trackColor 引用 @token：换 theme 对象即整体换配色
ThemeProvider({ theme: cyan },
    View({ padding: 12 }, [
        Slider({ value: 55, width: 280,
                 color: '@primary', trackColor: '@surfaceVariant' }),
    ]));`;

const demo4 = () => [
    ThemeProvider({ theme: cyan },
        View({ padding: [10, 12, 10, 12], background: '#F8FAFC', borderRadius: 8 }, [
            Slider({ value: 55, width: 280, color: '@primary', trackColor: '@surfaceVariant' }),
        ])),
    Text({ text: t(L.hint4), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C5 = `import { Slider, Button, Text, State, setProp, getProp } from 'kwikui';

const st = new State({ live: '0', read: '-' });

// onChange 全程实时回调（拖到哪触发到哪）
Slider({ id: 'live', min: 0, max: 100, width: 300,
        onChange: (e) => { st.live = String(e.value); } });
Text({ text: '⇒ ' + st.live });

// 非受控 + setProp 外部设值（clamp + step 取整后生效）
Slider({ id: 'imp', min: 0, max: 100, step: 5, width: 300 });
Button({ text: '设为 25', onClick: () => setProp('imp', 'value', 25) });
Button({ text: '读取', onClick: () => { st.read = getProp('imp', 'value'); } });`;

const demo5 = () => [
    Flex({ direction: 'row', alignItems: 'center', gap: 12 }, [
        Text({ text: t(L.live), width: 66, fontSize: 12.5, color: '#475569' }),
        Slider({
            min: 0, max: 100, step: 1, width: 260,
            color: '#059669', trackColor: '#E2E8F0',
            onChange: (e) => { st5.live = String(Math.round(e.value)); },
        }),
        Text({ text: '⇒ ' + st5.live, fontSize: 12.5, color: '#059669' }),
    ]),
    Flex({ direction: 'row', alignItems: 'center', gap: 10, margin: [14, 0, 0, 0] }, [
        Slider({
            id: 'ds-slider-imp', min: 0, max: 100, step: 5, width: 230,
            color: '#2563EB', trackColor: '#E2E8F0',
        }),
        btn(t(L.set25), () => setProp('ds-slider-imp', 'value', 25)),
        btn(t(L.set85), () => setProp('ds-slider-imp', 'value', 85)),
        btn(t(L.read), () => { st5.read = getProp('ds-slider-imp', 'value'); }, 104),
    ]),
    Text({ text: 'getProp ⇒ ' + st5.read, fontSize: 12, color: '#2563EB', margin: [8, 0, 0, 0] }),
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
