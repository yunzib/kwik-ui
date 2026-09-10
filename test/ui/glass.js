// 液态玻璃（Liquid Glass）测试示例
// 验证点：
//   ① 毛玻璃纯模糊      — backdropBlur
//   ② 液态玻璃          — backdropRefraction（边缘折射）+ backdropSpecular（边缘高光）
//   ③ 圆角 SDF          — borderRadius 在玻璃上生效、边缘干净无外扩光晕
//   ④ 变换下不错位      — 旋转/缩放玻璃的 UV 映射
//   ⑤ 裁剪内玻璃可见    — 圆角 overflow:hidden 容器内的玻璃（stencil/scissor 保活）
//   ⑥ 下层动画实时折射  — 小球在玻璃后往复运动，玻璃每帧重捕获
//   ⑦ 属性动画          — backdropBlur 补间
// 运行：example glass
import { Root, View, Text, Button, animate, stop } from 'kwikui';

const $ = globalThis.__glassState ??= {};

// ── 背景层：深底 + 彩色渐变块 + 文本（给玻璃提供可折射/可模糊的细节）──
const background = [
    // 大块渐变色斑（覆盖玻璃所在区域）
    View({ x: 0, y: 0, width: 1280, height: 800, background: "#14161f" }),
    View({ x: -60, y: 40, width: 420, height: 300, borderRadius: 60,
           gradient: "linear 45 #ff6b6b #ffd93d", opacity: 0.9 }),
    View({ x: 480, y: -40, width: 380, height: 260, borderRadius: 80,
           gradient: "linear 135 #4facfe #00f2fe", opacity: 0.85 }),
    View({ x: 900, y: 60, width: 340, height: 280, borderRadius: 50,
           gradient: "radial #f093fb #f5576c", opacity: 0.85 }),
    View({ x: 260, y: 560, width: 300, height: 260, borderRadius: 70,
           gradient: "linear 45 #43e97b #38f9d7", opacity: 0.8 }),
    View({ x: 760, y: 620, width: 420, height: 220, borderRadius: 60,
           gradient: "linear 135 #a18cd1 #fbc2eb", opacity: 0.8 }),

    // 密集文本条（模糊后软化明显，折射边缘弯折可见）
    ...Array.from({ length: 14 }, (_, i) =>
        Text({ x: 24, y: 250 + i * 38, text: '液态玻璃 Liquid Glass ── 折射/高斯/圆角SDF ── ' + i,
               fontSize: 15, color: i % 3 === 0 ? '#ffffffcc' : '#ffffff55' })),

    // 网格线（验证模糊量与折射方向的参照物）
    ...Array.from({ length: 16 }, (_, i) =>
        View({ x: i * 84, y: 240, width: 1, height: 560, background: '#ffffff14' })),

    // ⑥ 运动小球（绘制序在玻璃之前 = 玻璃背板内容；按钮启动动画）
    // 路径收在面板中部空旷区（按钮下方）：全程在玻璃后，运动以模糊/折射形态可见
    View({ id: 'glass-ball', x: 80, y: 520, width: 72, height: 72,
           borderRadius: 36, gradient: "radial #ffffff #ff930f", borderWidth: 2,
           borderColor: '#ffffff',
           shadow: '0 6px 18px rgba(0,0,0,0.5)' }),
];

// ── 样例行：四种玻璃参数对比 ──
const samples = [
    { x: 40,  label: '毛玻璃 blur=14',        props: { backdropBlur: 14 } },
    { x: 345, label: '液态 折射10/高光0.5',    props: { backdropBlur: 14, backdropRefraction: 10, backdropSpecular: 0.5 } },
    { x: 650, label: '强模糊 blur=40',         props: { backdropBlur: 40 } },
    { x: 955, label: '旋转8° 折射8（变换验证）', props: { backdropBlur: 12, backdropRefraction: 8, backdropSpecular: 0.35,
                                                          transform: '0,0,8,1.06' } },
];

export default Root(View({ width: 1280, height: 800 }, [
    ...background,

    // ── ①-④ 样例卡片 ──
    ...samples.map(s => View({
        x: s.x, y: 48, width: 285, height: 130, borderRadius: 18,
        background: '#ffffff0f',
        ...s.props,
    }, [
        Text({ text: s.label, fontSize: 13, color: '#ffffffee', x: 16, y: 10 }),
    ])),

    // ── 大液态玻璃面板：内容承载 + ⑥⑦ 交互 ──
    View({
        id: 'glass-panel', x: 40, y: 230, width: 560, height: 480, borderRadius: 24,
        background: '#ffffff12', borderWidth: 1, borderColor: '#ffffff30',
        backdropBlur: 18, backdropRefraction: 12, backdropSpecular: 0.55,
        padding: 24,
    }, [
        Text({ text: '液态玻璃面板', fontSize: 26, fontWeight: 'bold', color: '#ffffff' }),
        Text({ text: 'backdropBlur:18  refraction:12  specular:0.55', fontSize: 13,
               color: '#ffffffaa', margin: [0, 0, 6, 0] }),
        Text({ text: '下方小球在面板之后绘制，玻璃应实时折射其运动；'
                   + '按钮可切换小球动画与模糊强度补间。', fontSize: 14, color: '#ffffffcc' }),
        Button({ text: '启动/停止小球动画',
                 background: '#2196F3', color: '#FFFFFF', width: 220, height: 40,
                 borderRadius: 8, fontSize: 14, margin: [16, 0, 0, 0],
                 // 注：isAnimating() 为存根恒 false，此处用状态标志切换
                 onClick: () => {
                     if ($.ballOn) { stop('glass-ball'); $.ballOn = false; }
                     else { animate('glass-ball', { x: 500 }, { duration: 3.5, easing: 'easeInOut',
                                                                loop: true, direction: 'alternate' }); $.ballOn = true; }
                 } }),
        Button({ text: '模糊 18 ↔ 42 补间', background: '#FF5722', color: '#FFFFFF',
                 width: 220, height: 40, borderRadius: 8, fontSize: 14, margin: [10, 0, 0, 0],
                 onClick: () => {
                     $.strong = !$.strong;
                     animate('glass-panel', { backdropBlur: $.strong ? 42 : 18 }, { duration: 0.6 });
                 } }),
    ]),

    // ── ⑤ 圆角容器内的玻璃：验证 stencil/scissor 保活与捕获求交 ──
    // 注：引擎无 overflow 属性——borderRadius>0 即把子级裁剪到内容盒
    //     （frame 内缩 padding；首条彩条 x=-30 故意越界演示裁剪）
    View({ x: 640, y: 230, width: 600, height: 480, borderRadius: 28,
           background: '#0f1118', padding: 20 }, [
        Text({ text: '圆角容器（子级裁剪到内容盒，stencil/scissor）', fontSize: 15,
               fontWeight: 'bold', color: '#ffffffee', margin: [0, 0, 8, 0] }),
        Text({ text: '首条彩条故意越出容器左缘→被裁齐；玻璃卡片压住右下圆角，'
                   + '应完整可见且圆角边缘无拉丝。', fontSize: 13, color: '#ffffff99',
               margin: [0, 0, 12, 0] }),
        // 容器内彩色内容（玻璃的背板）；第一条 x=-30 越界 = 裁剪演示
        ...['#ff6b6b', '#ffd93d', '#4facfe', '#43e97b', '#f093fb', '#a18cd1'].map((c, i) =>
            View({ x: -30 + i * 96, y: 150, width: 70, height: 260, borderRadius: 12,
                   background: c, opacity: 0.9 })),
        ...Array.from({ length: 10 }, (_, i) =>
            Text({ x: 10 + i * 56, y: 130 + (i % 2) * 12, text: '裁剪' + i, fontSize: 12, color: '#ffffff' })),
        // 玻璃卡片：故意压住容器右下圆角
        View({ x: 300, y: 190, width: 260, height: 200, borderRadius: 20,
               background: '#ffffff14', backdropBlur: 16, backdropRefraction: 10,
               backdropSpecular: 0.4 }, [
            Text({ text: '裁剪内的玻璃', fontSize: 15, fontWeight: 'bold', color: '#ffffff',
                   x: 16, y: 12 }),
        ]),
    ]),

    // 底部提示
    Text({ x: 40, y: 745, text: '检查：圆角生效｜边缘无多余磨砂光晕｜模糊量≈props 值｜'
                             + '旋转卡片不错位｜裁剪内玻璃可见｜小球运动被实时折射',
           fontSize: 13, color: '#ffffff88' }),
]));
