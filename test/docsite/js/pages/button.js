// ── Button 文档页 ──
// 页契约：导出 meta（id / 分组 / 双语 title / 双语 desc / 集中示例代码
// 串 code）与内容自组装 content()（标题/描述/实况/代码的顺序与尺寸本页
// 定）+ demo()（实况卡内容，被 content() 调用）。代码卡与演示卡解耦：
// code 是给读者看的用法摘录，demo 是真实运行的实例。
// demo 内自持 State——点按 → State 写 → 框架重扩展（文本与按钮联动）。
import { View, Flex, Text, Button, State } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc, pageCode } from '../ui/page_parts.js';

export const meta = {
    id: 'button',
    group: 'base',
    title: { zh: 'Button 按钮', en: 'Button' },
    code: `import { View, Flex, Text, Button, State } from 'kwikui';

const st = new State({ n: 0 });

export const demo = () => {
    return View({}, [
        // 文本读 State → 每次点按计数随 reconcile 刷新
        Text({ text: '点击计数: ' + st.n }),
        Button({
            text: '主按钮',
            // 返回 true 可消费点击（阻止向祖先冒泡）
            onClick: () => { st.n = st.n + 1; },
        }),
    ]);
}`,
    desc: {
        zh: '承载一次点击操作的交互组件，支持文字颜色 / 圆角 / 尺寸自定义；'
            + 'onClick 返回 true 表示消费点击（阻止向祖先冒泡）。',
        en: 'An interactive component for click actions. Supports custom '
            + 'text color / radius / size; returning true from onClick '
            + 'consumes the tap (stops bubbling to ancestors).',
    },
};

const L = {
    count: { zh: '点击计数', en: 'Click count' },
    basic: { zh: '主按钮', en: 'Primary' },
    plain: { zh: '次按钮', en: 'Plain' },
    danger: { zh: '危险按钮', en: 'Danger' },
    hint: { zh: '每点一次计数 +1，观察上方文本联动。', en: 'Each click bumps the count shown above.' },
};

// State 必须模块顶层：demo() 每次重建都会重跑（State 写 → 整树 reconcile），
// 函数内 new State 每轮都是全新对象，计数永远回零
const st = new State({ n: 0 });

// 实况卡随内容区净宽伸缩（main 下传 innerW；默认 884 保持本文件可独立运行）
export const demo = (innerW = 884) => {
    const mkBtn = (label, bg, fg, onClick) => Button({
        text: t(label),
        width: 110, height: 34, fontSize: 13,
        background: bg, color: fg, borderRadius: 6,
        margin: [0, 12, 8, 0],
        onClick,
    });

    return View({
        width: innerW, padding: 16, background: '#FFFFFF',
        borderColor: '#E2E8F0', borderWidth: 1, borderRadius: 8,
        margin: [0, 0, 24, 4],
    }, [
        Text({
            text: t(L.count) + ': ' + st.n,
            fontSize: 14, color: '#0F172A',
        }),
        Flex({ direction: 'row', padding: [12, 0, 0, 0] }, [
            mkBtn(L.basic, '#ED6C02', '#FFFFFF', () => { st.n = st.n + 1; }),
            mkBtn(L.plain, '#EEF2F7', '#334155', () => { st.n = st.n + 1; }),
            mkBtn(L.danger, '#E5484D', '#FFFFFF', () => { st.n = 0; }),
        ]),
        Text({
            text: t(L.hint), fontSize: 12, color: '#909399',
            margin: [4, 0, 0, 0],
        }),
    ]);
};

// 页面自治：标题 + 描述 + 实况卡 + 代码卡（无属性表——本页未声明）
export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    demo(innerW),
    pageCode(meta, 320, innerW),
];
