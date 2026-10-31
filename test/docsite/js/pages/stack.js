// ── Stack 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/stack.md。
// 四个示例小节：居中与自适应包裹 / absolute 四角 / inset 拉伸 / z 顺序与偏移。
// 示例代码与演示同构：C1–C4 逐行镜像 demo1–demo4（改演示必须同步改代码，
// 反之亦然；同 flex / grid 页纪律）。
import { View, Text, Stack } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section, label } from '../ui/page_kit.js';

export const meta = {
    id: 'stack',
    group: 'layout',
    title: { zh: 'Stack 层叠', en: 'Stack' },
    desc: {
        zh: '所有子项在同一 Z 层堆叠：普通子项双向居中，容器不写宽高时按最大子项 + padding '
            + '自适应包裹；position:"absolute" 配 top / right / bottom / left 锚定四角或任意位置，'
            + '同轴两侧都设且不写显式尺寸则拉伸填满；z 控制同级层叠顺序，x / y 显式偏移两种定位都适用。',
        en: 'Children stack on one Z layer: plain children center both ways and the stack wraps '
            + 'its largest child when unsized; position:"absolute" with top / right / bottom / left '
            + 'pins corners or anywhere, stretching between both insets when no explicit size is set; '
            + 'z orders siblings and explicit x / y offsets apply to both positioning modes.',
    },
    props: [
        { title: { zh: '专有属性', en: 'Own props' }, rows: [
            ['—', '-', '-', { zh: '无专有属性（纯层叠容器）', en: 'None (pure stacking container)' }],
        ] },
        { title: { zh: '高频通用属性', en: 'Common ViewProps here' }, rows: [
            ['position', '"relative" | "absolute"', '"relative"', { zh: 'absolute 后启用四边 inset 定位', en: 'Enables inset positioning' }],
            ['top / right / bottom / left', 'number', '-', { zh: 'absolute 的四边 inset；同轴两侧都设且不写显式尺寸 → 拉伸', en: 'Insets; both set without explicit size stretches' }],
            ['x / y', 'number', '-', { zh: '显式偏移：居中 / 锚定结果上叠加，两种定位都适用', en: 'Explicit offset added after centering/anchoring' }],
            ['z', 'number', '0', { zh: '同级层叠顺序（越大越靠前，默认按声明序）', en: 'Sibling stacking order (higher = front)' }],
        ] },
    ],
};

const L = {
    s1: { zh: '居中层叠与自适应包裹', en: 'Centered stacking & auto-wrap' },
    s2: { zh: 'absolute 四角定位', en: 'Absolute corners' },
    s3: { zh: 'inset 拉伸', en: 'Stretch between insets' },
    s4: { zh: 'z 顺序与 x / y 偏移', en: 'z order & x / y offset' },
    hint1: { zh: '上：非绝对定位子项在内容区双向居中，声明顺序即 Z 序（后声明的画在上层）。'
            + '下：不写宽高时容器按最大子项 + padding 自适应包裹（180×90 + padding 8 → 196×106）',
             en: 'Top: non-absolute children center in the content area; declaration order is the '
             + 'Z order (later paints on top). Bottom: without width/height the stack wraps its '
             + 'largest child plus padding (180×90 + 8 → 196×106)' },
    hint2: { zh: 'position:"absolute" 后，top / right / bottom / left 相对内容区锚定：'
            + '只写一侧贴该侧，同轴两侧都不写则该轴居中（与普通子项一致）',
             en: 'With position:"absolute", top / right / bottom / left anchor to the content '
             + 'area: one side set pins to it; neither set centers on that axis (same as plain children)' },
    hint3: { zh: '同轴两侧 inset 都设且不写该轴显式尺寸 → 拉伸填满两 inset 之间：'
            + '本例顶 / 底栏水平通铺、左栏纵贯、主区四边全设双轴拉伸；'
            + '写了显式宽 / 高则只锚定不拉伸',
             en: 'Both insets on an axis without an explicit size stretch between them: the top '
             + 'and bottom bars span horizontally, the rail runs vertically and the main area '
             + 'stretches on both axes; an explicit width/height anchors without stretching' },
    hint4: { zh: '默认 Z 序 = 声明顺序（后者画在上层）；z 越大越靠前——蓝块最先声明却盖住后两块；'
            + '同 z 保持声明序。x / y 为显式偏移，在居中 / 锚定结果上叠加，absolute 与普通子项都适用',
             en: 'The default Z order is declaration order (later paints on top); higher z wins '
             + 'regardless — the blue block is declared first yet covers the other two; equal z '
             + 'keeps declaration order. x / y offset after centering/anchoring, in both modes' },
};

const C1 = `import { View, Text, Stack } from 'kwikui';

// 居中层叠：声明顺序即 Z 序（后声明的画在上层）
Stack({ width: 360, height: 180, background: '#F8FAFC', borderRadius: 8 }, [
    View({ width: 220, height: 130, background: '#DBEAFE', borderRadius: 12 }),
    View({ width: 140, height: 140, background: '#2563EB', borderRadius: 70, opacity: 0.85 }),
    Text({ text: 'STACK', fontSize: 18, fontWeight: 'bold', color: '#FFFFFF' }),
]);

// 不写宽高：容器按最大子项 + padding 自适应包裹（196×106）
Stack({ padding: 8, background: '#F8FAFC', borderRadius: 8, margin: [12, 0, 0, 0] }, [
    View({ width: 180, height: 90, background: '#DBEAFE', borderRadius: 10 }),
    Text({ text: 'auto size', fontSize: 12, color: '#475569' }),
]);`;

const demo1 = () => [
    Stack({ width: 360, height: 180, background: '#F8FAFC', borderRadius: 8 }, [
        View({ width: 220, height: 130, background: '#DBEAFE', borderRadius: 12 }),
        View({ width: 140, height: 140, background: '#2563EB', borderRadius: 70, opacity: 0.85 }),
        Text({ text: 'STACK', fontSize: 18, fontWeight: 'bold', color: '#FFFFFF' }),
    ]),
    Stack({ padding: 8, background: '#F8FAFC', borderRadius: 8, margin: [12, 0, 0, 0] }, [
        View({ width: 180, height: 90, background: '#DBEAFE', borderRadius: 10 }),
        Text({ text: 'auto size', fontSize: 12, color: '#475569' }),
    ]),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C2 = `import { View, Text, Stack } from 'kwikui';

// 小标签：dark 传 true 用深字（浅色块）
const label = (txt, dark) => Text({ text: txt, fontSize: 11.5,
                                    color: dark ? '#334155' : '#FFFFFF', padding: [4, 0, 0, 8] });

// 四角锚定：只写一侧贴该侧，两侧都不写的轴居中
Stack({ width: 360, height: 180, background: '#F8FAFC', borderRadius: 8 }, [
    View({ width: 54, height: 54, background: '#2563EB', borderRadius: 8,
           position: 'absolute', top: 12, left: 12 }, [label('TL')]),
    View({ width: 54, height: 54, background: '#16A34A', borderRadius: 8,
           position: 'absolute', top: 12, right: 12 }, [label('TR')]),
    View({ width: 54, height: 54, background: '#E5484D', borderRadius: 8,
           position: 'absolute', bottom: 12, left: 12 }, [label('BL')]),
    View({ width: 54, height: 54, background: '#F59E0B', borderRadius: 8,
           position: 'absolute', bottom: 12, right: 12 }, [label('BR')]),
]);`;

const demo2 = () => [
    Stack({ width: 360, height: 180, background: '#F8FAFC', borderRadius: 8 }, [
        View({ width: 54, height: 54, background: '#2563EB', borderRadius: 8,
               position: 'absolute', top: 12, left: 12 }, [label('TL')]),
        View({ width: 54, height: 54, background: '#16A34A', borderRadius: 8,
               position: 'absolute', top: 12, right: 12 }, [label('TR')]),
        View({ width: 54, height: 54, background: '#E5484D', borderRadius: 8,
               position: 'absolute', bottom: 12, left: 12 }, [label('BL')]),
        View({ width: 54, height: 54, background: '#F59E0B', borderRadius: 8,
               position: 'absolute', bottom: 12, right: 12 }, [label('BR')]),
    ]),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C3 = `import { View, Stack } from 'kwikui';

// label 见「absolute 四角定位」一节
// 同轴两侧都设且不写显式尺寸 → 拉伸：顶/底栏水平通铺、左栏纵贯、主区双轴拉伸
Stack({ width: 360, height: 220, background: '#F8FAFC', borderRadius: 8 }, [
    View({ position: 'absolute', top: 12, left: 12, right: 12, height: 40,
           background: '#BFDBFE', borderRadius: 8 }, [label('top+left+right', true)]),
    View({ position: 'absolute', bottom: 12, left: 12, right: 12, height: 32,
           background: '#FDE68A', borderRadius: 8 }, [label('bottom+left+right', true)]),
    View({ position: 'absolute', left: 12, top: 64, bottom: 56, width: 44,
           background: '#BBF7D0', borderRadius: 8 }, [label('rail', true)]),
    View({ position: 'absolute', left: 68, top: 64, bottom: 56, right: 12,
           background: '#E2E8F0', borderRadius: 8 }, [label('all four · stretch', true)]),
]);`;

const demo3 = () => [
    Stack({ width: 360, height: 220, background: '#F8FAFC', borderRadius: 8 }, [
        View({ position: 'absolute', top: 12, left: 12, right: 12, height: 40,
               background: '#BFDBFE', borderRadius: 8 }, [label('top+left+right', true)]),
        View({ position: 'absolute', bottom: 12, left: 12, right: 12, height: 32,
               background: '#FDE68A', borderRadius: 8 }, [label('bottom+left+right', true)]),
        View({ position: 'absolute', left: 12, top: 64, bottom: 56, width: 44,
               background: '#BBF7D0', borderRadius: 8 }, [label('left+top+bottom', true)]),
        View({ position: 'absolute', left: 68, top: 64, bottom: 56, right: 12,
               background: '#E2E8F0', borderRadius: 8 }, [label('all four · stretch', true)]),
    ]),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C4 = `import { View, Stack } from 'kwikui';

// label 见「absolute 四角定位」一节
// z：越大越靠前（蓝块最先声明却盖住后两块）；x / y：居中结果上叠加的显式偏移
Stack({ width: 360, height: 180, background: '#F8FAFC', borderRadius: 8 }, [
    View({ width: 120, height: 84, background: '#2563EB', borderRadius: 10,
           x: -44, y: -16, z: 2 }, [label('z 2')]),
    View({ width: 120, height: 84, background: '#16A34A', borderRadius: 10 }, [label('z 0')]),
    View({ width: 120, height: 84, background: '#E5484D', borderRadius: 10,
           x: 44, y: 16 }, [label('z 0 · 3rd')]),
]);`;

const demo4 = () => [
    Stack({ width: 360, height: 180, background: '#F8FAFC', borderRadius: 8 }, [
        View({ width: 120, height: 84, background: '#2563EB', borderRadius: 10,
               x: -44, y: -16, z: 2 }, [label('z 2')]),
        View({ width: 120, height: 84, background: '#16A34A', borderRadius: 10 }, [label('z 0')]),
        View({ width: 120, height: 84, background: '#E5484D', borderRadius: 10,
               x: 44, y: 16 }, [label('z 0 · 3rd')]),
    ]),
    Text({ text: t(L.hint4), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
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
