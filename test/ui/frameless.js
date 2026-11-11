// 无边框窗口验证页：自绘标题栏 + 拖拽区 + 最小化/最大化/关闭 + 任务栏图标
// 验证点：
//   ① 无边框：WS_THICKFRAME + WM_NCCALCSIZE 归零——标题栏自绘、系统装饰消失，
//      但保留四边/四角原生缩放带与 Aero Snap（贴边/拖到顶最大化）
//   ② 拖拽区：setDragRegions 声明的标题栏空白条 → HTCAPTION，按住拖动移动窗口，
//      双击 = 最大化/还原（系统语义）
//   ③ 控制按钮：minimizeWindow / toggleMaximizeWindow / closeWindow（JS 命令面）
//   ④ 状态事件：onWindowStateChange（最大化/还原时状态行与提示同步）
//   ⑤ 任务栏图标：窗口创建选项 iconPath（resources/icons/kwik.ico）——
//      任务栏 / Alt-Tab / 标题栏同图标
// 运行：example.exe frameless
import { Root, View, Flex, Text, Button, State, ref, getWindowSize,
         setDragRegions, minimizeWindow, toggleMaximizeWindow, closeWindow } from 'kwikui';

const TITLEBAR_H = 40;
const BTN_W = 44;
const BTN_COUNT = 3;

const st = new State({ hint: "拖动标题栏移动 · 双击最大化/还原 · 边缘缩放 · 任务栏图标", max: false });

// ── 拖拽区（无边框窗口的"HTCAPTION 区"）────────────────────────────────
// Win32 对鼠标位置做 WM_NCHITTEST 判定：命中拖拽区 → HTCAPTION（系统按
// "点住标题栏"处理：按住拖动移动窗口、双击最大化/还原）。系统标题栏已被
// WS_POPUP 去掉，所以这里显式声明"哪块客户区算标题栏"。
//   · 坐标 = 窗口逻辑像素（左上角原点，与 getWindowSize 同域）；平台在
//     WM_NCHITTEST 里按当前屏 S₀ 换算物理坐标（window_win32.cpp）。
//   · 宽度扣除右侧 3 枚控制按钮（44×3=132px）：HTCAPTION 会抢走点击，
//     不扣掉按钮区域则 —/□/× 永远收不到 onClick。Math.max(0, …) 防窄窗负数。
//   · 窗口最外侧 6px 缩放带优先于拖拽区（边缘可缩放），拖拽条贴顶边仍可拖顶边。
//   · 可传多个矩形；空数组 = 清除。窗口 resize 后需重调（宽度随窗口同步）。
function syncDragRegions(w) {
    setDragRegions([{ x: 0, y: 0, width: Math.max(0, w - BTN_W * BTN_COUNT), height: TITLEBAR_H }]);
}

// 模块加载即声明一次（窗口在 JS 求值前已完成注册）
syncDragRegions(getWindowSize().width);

// 标题栏控制按钮（背景显式给出 = 覆盖 Button 主题 primary 底）
const ctrlBtn = (text, bg, onClick) => Button({
    text, width: BTN_W, height: 32, fontSize: 14, color: "#E2E8F0", background: bg,
    margin: [4, 0, 0, 0], onClick
});

// 说明卡片
const panel = (title, desc) => View({
    flexGrow: 1, height: 132, background: "#1E293B", borderRadius: 10,
    padding: [16, 16, 16, 16], margin: [0, 0, 0, 0],
}, [
    Text({ text: title, fontSize: 15, color: "#F1F5F9", margin: [0, 0, 8, 0] }),
    Text({ text: desc, fontSize: 12, color: "#94A3B8" }),
]);

export default () => {
    const ws = getWindowSize();
    syncDragRegions(ws.width);    // 重建后按当前窗口宽度刷新（resize 已更新 State）
    return Root({
            // 窗口事件（根直投）：尺寸/状态变化只刷新提示行与拖拽区，业务内容不动
            onWindowResize: (e) => {
                syncDragRegions(e.width);
                st.update({ hint: `窗口 ${e.width}×${e.height} · 拖动标题栏移动 · 双击最大化` });
            },
            onWindowStateChange: (e) => st.update({
                max: e.maximized,
                hint: e.maximized ? "已最大化：双击标题栏或点 □ 还原" : "拖动标题栏移动 · 双击最大化/还原 · 边缘缩放",
            }),
        },
        View({ width: ws.width, height: ws.height, background: "#0F172A" }, [
            // ── 自绘标题栏（拖拽区在标题栏空白处；按钮区域不计入）──
            Flex({ direction: "row", height: TITLEBAR_H, background: "#1E293B" }, [
                Text({ text: "KwiK UI · Frameless", fontSize: 14, color: "#E2E8F0", margin: [11, 12, 0, 0] }),
                View({ flexGrow: 1 }),
                ctrlBtn("—", "#334155", () => minimizeWindow()),
                ctrlBtn("□", "#334155", () => toggleMaximizeWindow()),
                ctrlBtn("×", "#DC2626", () => closeWindow()),
            ]),
            // ── 内容区 ──
            View({ padding: [24, 24, 24, 24] }, [
                Text({ text: "无边框窗口（Borderless）", fontSize: 22, color: "#F1F5F9", margin: [8, 0, 8, 0] }),
                Text({ text: ref(st, "hint"), fontSize: 13, color: "#94A3B8", margin: [8, 0, 20, 0] }),
                Flex({ direction: "row", gap: 12 }, [
                    panel("拖拽", "按住标题栏空白处拖动窗口；双击 = 最大化/还原（HTCAPTION 系统语义）"),
                    panel("缩放", "窗口四边/四角 6px 原生缩放带；拖到屏幕边缘触发 Aero Snap"),
                    panel("图标", "任务栏 / Alt-Tab / 标题栏显示 resources/icons/kwik.ico"),
                ]),
            ]),
        ])
    );
};
