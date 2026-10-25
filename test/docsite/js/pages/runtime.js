// ── 运行时与生命周期 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/8. 运行时与生命周期.md。
// 两个示例小节：定时器 + onMount/onUnmount 清理 / requestAnimationFrame 节奏说明。
import { View, Flex, Text, Button, State,
    setInterval, clearInterval } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section, note } from '../ui/page_kit.js';

export const meta = {
    id: 'runtime',
    group: 'guide',
    title: { zh: '运行时与生命周期', en: 'Runtime & Lifecycle' },
    desc: {
        zh: '框架宿主 API：帧驱动的 setTimeout / setInterval、逐帧 requestAnimationFrame，'
            + '以及组件 onMount / onUnmount 生命周期；切页或关窗时引擎自动清场。',
        en: 'Host APIs: frame-driven setTimeout / setInterval, requestAnimationFrame, and component '
            + 'onMount / onUnmount lifecycle hooks. The engine cleans up on page/window teardown.',
    },
    props: [
        { title: { zh: '宿主 API', en: 'Host API' }, rows: [
            ['setTimeout / setInterval', '(fn, ms) → id', '-', { zh: '帧驱动；短延时受帧粒度限制', en: 'Frame-driven; short delays quantized to frames' }],
            ['clearTimeout / clearInterval', '(id)', '-', { zh: '共用同一 id 空间', en: 'Shared id space' }],
            ['requestAnimationFrame', '(fn) → id', '-', { zh: '下一帧执行一次；ts 为毫秒时刻', en: 'Runs once next frame; ts in ms' }],
            ['cancelAnimationFrame', '(id)', '-', { zh: '取消尚未执行的回调', en: 'Cancel a pending callback' }],
        ] },
        { title: { zh: '生命周期', en: 'Lifecycle' }, rows: [
            ['onMount', '() => void', '-', { zh: '新节点挂载后前序触发；复用节点不重触发', en: 'New nodes only, pre-order' }],
            ['onUnmount', '() => void', '-', { zh: '节点拆除前后序触发；清理定时器 / 订阅', en: 'Before teardown, post-order; clean timers/subscriptions' }],
        ] },
    ],
};

const L = {
    s1: { zh: '定时器与生命周期清理', en: 'Timers & lifecycle cleanup' },
    s2: { zh: 'rAF 逐帧动画', en: 'requestAnimationFrame' },
    start: { zh: '开始计时', en: 'Start timer' },
    stop: { zh: '停止', en: 'Stop' },
    ticks: { zh: '计时次数', en: 'Ticks' },
    mounted: { zh: '本卡随页面挂载 / 卸载自动清理定时器。', en: 'This card cleans its timer on unmount.' },
};

const st = new State({ ticks: 0, running: false });
let timer = 0;
const startTicks = () => {
    if (timer) return;
    st.running = true;
    timer = setInterval(() => { st.ticks = st.ticks + 1; }, 500);
};
const stopTicks = () => {
    if (timer) { clearInterval(timer); timer = 0; }
    st.running = false;
};

const C1 = `let timer = 0;
const ui = new State({ ticks: 0 });

View({
    onMount: () => {
        timer = setInterval(() => { ui.ticks = ui.ticks + 1; }, 500);
    },
    onUnmount: () => {
        clearInterval(timer);
        timer = 0;
    },
}, [Text({ text: 'ticks: ' + ui.ticks })]);`;
const demo1 = () => [
    View({
        onMount: () => { console.log('runtime demo mounted'); },
        onUnmount: () => { stopTicks(); console.log('runtime demo unmounted'); },
    }, [
        Flex({ direction: 'row', alignItems: 'center', gap: 12 }, [
            Text({ text: t(L.ticks) + ': ' + st.ticks, fontSize: 13, color: '#0F172A', width: 120 }),
            Button({
                text: t(L.start), width: 100, height: 30, fontSize: 12,
                background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
                onClick: startTicks,
            }),
            Button({
                text: t(L.stop), width: 80, height: 30, fontSize: 12,
                background: '#EEF2F7', color: '#334155', borderRadius: 6,
                onClick: stopTicks,
            }),
        ]),
        note(t(L.mounted), 420),
    ]),
];

const C2 = `function step(ts) {
    const p = Math.min(1, (ts - start) / 500);
    ui.progress = 300 * p;
    if (p < 1) requestAnimationFrame(step);
}
requestAnimationFrame(step);`;
const demo2 = () => [
    Text({ text: 'rAF 只保证下一帧执行一次；连续动画在回调末尾再次注册。', fontSize: 12, color: '#475569' }),
    Text({ text: '用时间戳 ts 计算进度，帧率变化不会改变动画速度。', fontSize: 12, color: '#475569', margin: [0, 4, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    propsTable(t, meta.props, innerW),
];
