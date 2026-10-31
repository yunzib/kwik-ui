// ── Spinner 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/spinner.md。
// 四节结构镜像 test/ui/spinner.js 验证 demo（默认 / 颜色 / 尺寸 / visible 控制）。
// 说明：弧段外"尾迹"圆点由主色低透明度生成（onDraw 取 color·alpha），props.cppm
// 里的 trackColor 仅解析、绘制未消费——页面 props 表不再列它（待 C++ 决断）。
// demo 侧改动必须同步本页（reverse 也是）。
import { Flex, Text, Spinner, Button, getProp, setProp } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'spinner',
    group: 'progress',
    title: { zh: 'Spinner 加载指示', en: 'Spinner' },
    desc: {
        zh: '12 个圆点沿轨道旋转的加载指示（lvgl 风）：弧段内高亮、其余低亮成尾迹，'
            + '头尾各 15% 弧长渐变收边；创建即自动持续旋转，无需用户交互。',
        en: 'Twelve dots rotating on a track (lvgl style): a highlighted arc with a faint tail, '
            + 'fading over 15% at both ends; it animates automatically without interaction.',
    },
    props: [
        { title: { zh: '专有属性', en: 'Props' }, rows: [
            ['color', '#rrggbb', '#1976D2', { zh: '圆点主色（弧段高亮 + 尾迹低透明同源）', en: 'Dot color (drives arc and dimmed tail)' }],
            ['size', 'number', '32', { zh: '外接圆直径 px', en: 'Overall diameter px' }],
            ['strokeWidth', 'number', '6', { zh: '圆点直径 px（轨道半径 = (size-strokeWidth)/2）', en: 'Dot diameter px (orbit = (size-strokeWidth)/2)' }],
            ['arcLength', 'number (0..360)', '200', { zh: '高亮弧段角（头尾各 15% 渐变收边）', en: 'Highlighted arc angle (15% fade at both ends)' }],
        ] },
    ],
};

const L = {
    s1: { zh: '默认（32px）', en: 'Default (32px)' },
    s2: { zh: '自定义颜色', en: 'Custom colors' },
    s3: { zh: '自定义尺寸', en: 'Custom sizes' },
    s4: { zh: '显示控制（setProp visible）', en: 'Visibility (setProp visible)' },
    toggle: { zh: '隐藏 / 显示', en: 'Hide / show' },
    hint1: { zh: '默认 32px · 蓝 #1976D2；旋转自动播放（onDraw 逐帧自增相位，约 2.6s 一圈 @60fps），'
               + '组件创建即转，无任何交互。',
            en: 'Default 32px blue; rotation is automatic (phase advances per frame, ≈2.6s per '
               + 'revolution at 60fps) — no interaction needed.' },
    hint2: { zh: '颜色即主色：弧段外圆点取主色 15% 亮度成尾迹，弧段内头尾各 15% 弧长渐变融合——'
               + '换色整体联动（无独立轨道色属性参与绘制）。',
            en: 'One color drives all: out-of-arc dots dim to 15% for the tail, arcs fade over 15% '
               + 'at both ends — no separate track color participates in drawing.' },
    hint3: { zh: 'size 是外接圆直径；strokeWidth 增大时轨道半径同步收缩'
               + '（orbit = (size - strokeWidth) / 2，圆点贴环不越界）。',
            en: 'size is the overall diameter; a larger strokeWidth shrinks the orbit '
               + '(orbit = (size - strokeWidth) / 2).' },
    hint4: { zh: 'visible 是所有组件的通用属性：getProp 类型化返回原生布尔（true/false，'
               + '非字符串——与 "true" 比较恒 false 是老陷阱），setProp 接受字符串布尔；'
               + '隐藏后不参与测量与布局（不占位）。',
            en: 'visible is a common prop on every widget: getProp returns a native boolean '
                + '(typed return — comparing against the string "true" is always false), '
                + 'setProp takes string booleans; hidden widgets skip measure and layout.' },
};

// ── 控制按钮统一样式（gauge/progressring 页同款：语义色 + hover 变亮 + pressed 变深）──
const makeBtn = (label, color, onClick) => Button({
    text: label, width: 120, height: 32, fontSize: 13,
    background: color,
    color: '#FFFFFF',    // TextContent 键是 color（textColor 不存在，静默忽略后点击 rebuild 即翻黑）
    borderRadius: 6,
    hoverBackground: [100, 190, 240, 255],
    pressedBackground: [13, 71, 161, 255],
    onClick,
});

// ── ① 默认（demo 首节镜像）──
const demo1 = () => [
    Flex({ direction: 'row', gap: 32, alignItems: 'center', margin: [0, 0, 24, 0] }, [
        Spinner(),
    ]),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
];

// ── ② 自定义颜色（demo 次节镜像：四色并排）──
const demo2 = () => [
    Flex({ direction: 'row', gap: 32, alignItems: 'center', margin: [0, 0, 24, 0], flexWrap: 'wrap' }, [
        Spinner({ color: '#4CAF50' }),
        Spinner({ color: '#FF5252' }),
        Spinner({ color: '#FF9800' }),
        Spinner({ color: '#9C27B0' }),
    ]),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
];

// ── ③ 自定义尺寸（demo 第三节镜像：四档 size/strokeWidth）──
const demo3 = () => [
    Flex({ direction: 'row', gap: 32, alignItems: 'center', margin: [0, 0, 24, 0], flexWrap: 'wrap' }, [
        Spinner({ size: 24, strokeWidth: 4 }),
        Spinner({ size: 32, strokeWidth: 4 }),
        Spinner({ size: 48, strokeWidth: 6 }),
        Spinner({ size: 64, strokeWidth: 8 }),
    ]),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
];

// ── ④ 显示控制（demo 收节镜像：getProp 读 → setProp 写 visible）──
const demo4 = () => [
    Flex({ direction: 'row', gap: 16, alignItems: 'center', margin: [0, 0, 24, 0] }, [
        makeBtn(t(L.toggle), [33, 150, 243, 255], () => {
            // visible 属 bool 描述符属性：getProp 类型化返回原生布尔（非字符串），
            // 须与 true 比较——字符串比较恒 false 会使翻转永写 'true'（隐藏失效）
            const v = getProp('ds-sp-1', 'visible');
            setProp('ds-sp-1', 'visible', v === true ? 'false' : 'true');
        }),
        Spinner({ id: 'ds-sp-1', color: '#1976D2', size: 32 }),
    ]),
    Text({ text: t(L.hint4), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
];

// 代码卡（与上方演示逐行同源；改动演示同步这里）
const C1 = `// 默认 32px · 蓝 #1976D2；创建即自转（无需交互）
Spinner();`;

const C2 = `// 颜色即主色：弧段高亮与尾迹低透明同源
Spinner({ color: '#4CAF50' });
Spinner({ color: '#FF5252' });
Spinner({ color: '#FF9800' });
Spinner({ color: '#9C27B0' });`;

const C3 = `// size = 外接圆直径；strokeWidth = 圆点直径
// （轨道半径 = (size - strokeWidth) / 2，点贴环不越界）
Spinner({ size: 24, strokeWidth: 4 });
Spinner({ size: 32, strokeWidth: 4 });
Spinner({ size: 48, strokeWidth: 6 });
Spinner({ size: 64, strokeWidth: 8 });`;

const C4 = `// visible：所有组件通用属性（隐藏后不测不占位）
Spinner({ id: 'ds-sp-1', color: '#1976D2', size: 32 });

// 演示钮即此翻转（bool 属性 getProp 类型化返回原生布尔，与 "true" 字符串比较恒不等）
const v = getProp('ds-sp-1', 'visible');           // true / false
setProp('ds-sp-1', 'visible', v === true ? 'false' : 'true');`;

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    ...section(t(L.s3), demo3(), C3, innerW),
    ...section(t(L.s4), demo4(), C4, innerW),
    propsTable(t, meta.props, innerW),
];
