import { View, Text, State, ref } from 'kwikui';

// 状态驱动：背景色 / 旋转角度 / 垂直位移
const state = new State({ bg: "#4facfe", rot: 0, ty: 0 });

export default View({
    id: "root", width: 800, height: 600,
    background: "#f5f5f5", padding: 24,
}, [
    Text({ text: "隐式 transition 测试（transitionDuration）", fontSize: 22, color: "#333", margin: [0, 0, 16, 0] }),
    Text({ text: "点击卡片 → state 变化 → 背景/旋转/位移自动补间 0.5s；右侧卡片无过渡对比跳变", fontSize: 15, color: "#666", margin: [0, 0, 24, 0] }),

    // 有 transitionDuration → 平滑过渡
    View({
        id: "card", x: 80, y: 140, width: 240, height: 140, borderRadius: 12,
        background: ref(state, "bg"),
        rotate: ref(state, "rot"),
        translateY: ref(state, "ty"),
        transitionDuration: 0.5,
        onClick: () => {
            state.bg  = state.bg  === "#4facfe" ? "#ff6b6b" : "#4facfe";
            state.rot = state.rot === 0 ? 12 : 0;
            state.ty  = state.ty  === 0 ? 20 : 0;
        },
    }, [
        Text({ text: "点击我（过渡）", fontSize: 20, color: "#ffffff", x: 66, y: 56 }),
    ]),

    // 无 transitionDuration → 直接跳变（对比）
    View({
        id: "cardNoTrans", x: 400, y: 140, width: 240, height: 140, borderRadius: 12,
        background: ref(state, "bg"),
        onClick: () => {
            state.bg = state.bg === "#4facfe" ? "#ff6b6b" : "#4facfe";
        },
    }, [
        Text({ text: "无过渡（对比）", fontSize: 20, color: "#ffffff", x: 62, y: 56 }),
    ]),
]);