// ── Spinner 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/spinner.md。
// 单演示小节：三种尺寸 / 颜色的加载指示。
import { View, Flex, Text, Spinner } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'spinner',
    group: 'adv',
    title: { zh: 'Spinner 加载指示', en: 'Spinner' },
    desc: {
        zh: '12 个圆点沿轨道旋转的加载动画，弧段内高亮、其余低亮形成尾迹；'
            + '创建后自动持续旋转，无需用户交互。',
        en: 'Twelve dots rotating on a track, with a highlighted arc and faint tail; '
            + 'it animates automatically without interaction.',
    },
    props: [
        { title: { zh: '专有属性', en: 'Props' }, rows: [
            ['color', '#rrggbb', '#1976D2', { zh: '圆点颜色', en: 'Dot color' }],
            ['size', 'number', '32', { zh: '整体尺寸 px', en: 'Overall size' }],
            ['strokeWidth', 'number', '6', { zh: '圆点直径 px', en: 'Dot diameter' }],
            ['trackColor', '#rrggbb', 'color 15%', { zh: '背景圆点颜色', en: 'Track dot color' }],
            ['arcLength', 'number (0..360)', '200', { zh: '高亮弧段角度', en: 'Highlighted arc angle' }],
        ] },
    ],
};

const L = {
    s1: { zh: '尺寸与颜色', en: 'Size & color' },
};

const C1 = `Spinner({
    size: 36,
    strokeWidth: 6,
    color: '#2563EB',
    arcLength: 200,   // 高亮弧段角度
});`;
const demo1 = () => [
    Flex({ direction: 'row', alignItems: 'center', gap: 30 }, [
        Spinner({ size: 24, strokeWidth: 4, color: '#2563EB', arcLength: 200 }),
        Spinner({ size: 36, strokeWidth: 6, color: '#16A34A', arcLength: 220 }),
        Spinner({ size: 48, strokeWidth: 8, color: '#E5484D', trackColor: '#FEE2E2', arcLength: 240 }),
    ]),
    Text({ text: t(L.s1), fontSize: 11, color: '#94A3B8', margin: [2, 8, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
