// 定时器 + 生命周期钩子演示（路线图第 2 项: setTimeout/setInterval/rAF/onMount/onUnmount）
// API 经 kwikui 模块导出；定时器为帧驱动语义（setTimeout(fn,0) ≈ 下一帧），
// interval 重载时推迟不堆积；rAF 在每帧 flush 后执行，改 State 当帧生效。
// HMR/关窗时定时器随 Channel::shutdown 全部清空（框架语义）。
import {
    View, Text, Button, State,
    setTimeout, setInterval, clearInterval, requestAnimationFrame,
} from 'kwikui';

const state = new State({
    intervalCount: 0, rafCount: 0,
    timerMsg: 'setTimeout 未触发',
    lifecycleMsg: '等待挂载',
    intervalOn: false, showChild: false,
});

let intervalId = 0;
let rafId = 0;
let mountCount = 0;
let unmountCount = 0;

function startTimers() {
    // setInterval: 每 200ms 计数；setTimeout 1.5s 后清理（演示清理锚点配对）
    intervalId = setInterval(() => { state.intervalCount = state.intervalCount + 1; }, 200);
    state.intervalOn = true;
    setTimeout(() => {
        clearInterval(intervalId);
        state.intervalOn = false;
        state.timerMsg = 'interval 已清（onUnmount 即此锚点的自动化）';
    }, 1500);
    // rAF: 每帧自续
    const loop = () => {
        state.rafCount = state.rafCount + 1;
        rafId = requestAnimationFrame(loop);
    };
    rafId = requestAnimationFrame(loop);
}

// 条件子组件: 挂载/卸载时更新计数（验证 firePendingMounts 与拆除点通知）
function childCard() {
    return View({
        padding: 10, background: '#161b22', borderRadius: 6,
        borderWidth: 1, borderColor: '#1f6feb',
        onMount: () => {
            mountCount++;
            state.lifecycleMsg = '子组件 onMount #' + mountCount;
        },
        onUnmount: () => {
            unmountCount++;
            state.lifecycleMsg = '子组件 onUnmount #' + unmountCount;
        },
    }, [
        Text({ text: '条件子组件（挂载/卸载观察）', fontSize: 13, color: '#58a6ff' }),
        Text({ text: '父级拆除我时 onUnmount 在析构前触发', fontSize: 11, color: '#8b949e' }),
    ]);
}

export default () => View({
    width: 460, height: 340,
    background: '#0d1117', padding: 14, gap: 8,
    onMount: () => {
        state.lifecycleMsg = '根组件 onMount（树构建完成后触发）';
        startTimers();
    },
}, [
    Text({ text: 'TIMER & LIFECYCLE', fontSize: 18, fontWeight: 'bold', color: '#e6edf3' }),
    Text({ text: 'rAF 帧计数: ' + state.rafCount, fontSize: 13, color: '#f0c674' }),
    Text({
        text: 'setInterval 计数: ' + state.intervalCount + (state.intervalOn ? '（运行中）' : '（已清）'),
        fontSize: 13, color: '#3fb950',
    }),
    Text({ text: state.timerMsg, fontSize: 12, color: '#8b949e' }),
    Text({ text: state.lifecycleMsg, fontSize: 12, color: '#d2a8ff' }),
    Button({
        text: '切换条件子组件',
        onClick: () => { state.showChild = !state.showChild; },
    }),
    ...(state.showChild ? [childCard()] : []),
]);
