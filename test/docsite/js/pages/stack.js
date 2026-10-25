// ── Stack 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/stack.md。
// 两个示例小节：居中层叠 / absolute 四角定位。
import { View, Flex, Text, Stack } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'stack',
    group: 'layout',
    title: { zh: 'Stack 层叠', en: 'Stack' },
    desc: {
        zh: '所有子项在同一 Z 层堆叠，默认居中；配合 position:"absolute" 与 '
            + 'top / right / bottom / left 可做四角或任意位置精确摆放。',
        en: 'Children stack on the same Z layer and center by default; '
            + 'with position:"absolute" and top / right / bottom / left they can be placed precisely.',
    },
    props: [
        { title: { zh: '专有属性', en: 'Props' }, rows: [
            ['—', '-', '-', { zh: '无专有属性；定位依赖通用 ViewProps 的 position:absolute + top/left/right/bottom', en: 'No own props; use position:absolute + inset props' }],
            ['z', 'number', '0', { zh: '同级层叠顺序（越大越靠前）', en: 'Sibling stacking order' }],
        ] },
    ],
};

const L = {
    s1: { zh: '居中层叠', en: 'Centered stacking' },
    s2: { zh: 'absolute 四角定位', en: 'Absolute corners' },
};

const C1 = `Stack({ width: 360, height: 180 }, [
    View({ width: 220, height: 130, background: '#DBEAFE', borderRadius: 12 }),
    View({ width: 140, height: 140, background: '#2563EB', borderRadius: 70 }),
    Text({ text: 'STACK', color: '#FFFFFF' }),
]);`;
const demo1 = () => [
    Stack({ width: 360, height: 180, background: '#F8FAFC', borderRadius: 8 }, [
        View({ width: 220, height: 130, background: '#DBEAFE', borderRadius: 12 }),
        View({ width: 140, height: 140, background: '#2563EB', borderRadius: 70, opacity: 0.85 }),
        Text({ text: 'STACK', fontSize: 18, fontWeight: 'bold', color: '#FFFFFF' }),
    ]),
];

const C2 = `Stack({ width: 360, height: 180 }, [
    View({ width: 54, height: 54, background: '#2563EB',
           position: 'absolute', top: 12, left: 12 }),
    View({ width: 54, height: 54, background: '#16A34A',
           position: 'absolute', top: 12, right: 12 }),
]);`;
const demo2 = () => [
    Stack({ width: 360, height: 180, background: '#F8FAFC', borderRadius: 8 }, [
        View({ width: 54, height: 54, background: '#2563EB', borderRadius: 8, position: 'absolute', top: 12, left: 12 }),
        View({ width: 54, height: 54, background: '#16A34A', borderRadius: 8, position: 'absolute', top: 12, right: 12 }),
        View({ width: 54, height: 54, background: '#E5484D', borderRadius: 8, position: 'absolute', bottom: 12, left: 12 }),
        View({ width: 54, height: 54, background: '#F59E0B', borderRadius: 8, position: 'absolute', bottom: 12, right: 12 }),
    ]),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    propsTable(t, meta.props, innerW),
];
