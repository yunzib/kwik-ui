// ── 首页（快上手）──────
// 文档站着陆：一句话说明 + 特性卡片栅格。内容由本页 content() 自组装
// （meta.code 为集中式快上手摘录，经 ui/page_parts 的 pageCode 挂出）。
import { View, Flex, Text } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc, pageCode } from '../ui/page_parts.js';

export const meta = {
    id: 'index',
    group: 'docs',
    title: { zh: 'KwiK UI 文档', en: 'KwiK UI Docs' },
    code: `import { Root, View, Text, Button, State } from 'kwikui';

    const st = new State({ n: 0 });

    export default () => Root(View({ width: 1280, height: 820 }, [
        // 文本读 State，点按触发整树 reconcile
        Text({ text: '点击数: ' + st.n }),
        Button({ text: '点我', onClick: () => { st.n = st.n + 1; } }),
    ]));`,
    desc: {
        zh: '声明式 C++/JS 组件库的交互式文档：左侧导航切换组件，'
            + '每页含可交互示例与示例代码；点右上角按钮切换双语。',
        en: 'Interactive docs for a declarative C++/JS UI library: '
            + 'pick a component in the sidebar; each page has a live demo '
            + 'and example code. Toggle language from the header.',
    },
};

const CARDS = [
    {
        k: { zh: '声明式组合', en: 'Declarative composition' },
        v: {
            zh: '组件 = 工厂调用：props 对象 + children 数组；状态驱动自更新，另备 getProp/setProp 命令式通道按需补位。',
            en: 'Components are factory calls: props object + children array — state writes update the UI on their own, with imperative getProp/setProp as the escape hatch.',
        },
    },
    {
        k: { zh: 'State 响应', en: 'Reactive State' },
        v: {
            zh: 'JS 侧 State 写 → 框架整树 reconcile，派生 UI 自动更新。',
            en: 'JS State writes trigger full-tree reconcile; derived UI updates automatically.',
        },
    },
    {
        k: { zh: '布局体系', en: 'Layout system' },
        v: {
            zh: 'Flex/Grid/List/Stack/ScrollView/LazyList 容器族：弹性换行、可滚动区、虚拟化列表开箱即用。',
            en: 'Flex/Grid/List/Stack/ScrollView/LazyList containers: wrapping flow, scrollables and virtualized lists out of the box.',
        },
    },
    {
        k: { zh: '动画与过渡', en: 'Animation' },
        v: {
            zh: 'animate() 时间驱动 + transition 隐式过渡：绑定派生的属性变化框架自动补间，rAF/定时器面齐备。',
            en: 'Keyframe animate() plus implicit transition tweening on bound props, with rAF/timer utilities included.',
        },
    },
    {
        k: { zh: 'Channel 跨语言', en: 'Channel bridge' },
        v: {
            zh: 'JS↔C++ 双向消息：send/on 推送，call/handle 请求-回执，跨层通信不轮询。',
            en: 'Two-way JS↔C++ messaging: send/on pushes, call/handle request-reply — no polling across layers.',
        },
    },
    {
        k: { zh: '热重载', en: 'Hot reload' },
        v: {
            zh: 'Debug 模式下改 JS 文件保存即重载，页面不丢失。',
            en: 'In debug builds, saving a JS file reloads the app instantly.',
        },
    },
];

// 特性卡随内容区净宽伸缩（main 下传 innerW；默认 884 保持本文件可独立运行）
export const demo = (innerW = 884) => {
    const cardW = (innerW - 12) / 2;    // 卡宽 = 净宽半分扣 12 间距；窄窗自动断行为单列
    const textW = cardW - 28;           // 卡 padding 14×2 → 卡内文本可用宽
    return View({ width: innerW }, [
        // 特性卡 2×3：View 是纵向单列流，两列排列必须显式 Flex wrap
        //（行主轴 innerW 内 cardW+12+cardW 放两列，超出自动断行）
        Flex({ direction: 'row', flexWrap: 'wrap', width: innerW }, CARDS.map(c => View({
            width: cardW, padding: 14, background: '#FFFFFF',
            borderColor: '#E2E8F0', borderWidth: 1, borderRadius: 8,
            margin: [0, 12, 12, 4],
        }, [
                Text({ text: t(c.k), fontSize: 14, color: '#0F172A', margin: [0, 0, 6, 0] }),
                Text({ text: t(c.v), fontSize: 12, color: '#64748B', wordWrap: true, width: textW }),
        ]))),
    ]);
};

// 页面自治：标题 + 描述 + 特性卡栅格 + 本页代码卡（顺序/尺寸本页说了算）
export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    demo(innerW),
    pageCode(meta, 320, innerW),
];
