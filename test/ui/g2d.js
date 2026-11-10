/**
 * G2D 2D 绘制综合示例（12 画布全景演示）
 *
 * 覆盖全部 API：
 *   矩形：fillRect / strokeRect / clearRect（含渐变画刷变体）
 *   路径：beginPath / moveTo / lineTo / quadraticCurveTo / bezierCurveTo
 *         arc / ellipse / closePath / fill / stroke
 *   文本：fillText（TextRenderPipeline 同管线）
 *   渐变：createLinearGradient / createRadialGradient（fillRect 走真渐变）
 *   变换：translate / rotate / scale（save/restore 弹压）
 *   状态：save / restore / setFillStyle / setStrokeStyle / setLineWidth /
 *         setGlobalAlpha / reset
 *   动画：requestAnimationFrame + reset() 逐帧重录（旋转/轨道/脉冲）
 *
 * 运行：example.exe g2d
 *
 * 布局预算（Root 1280×800, padding 24）：标题 24 + 4 行 × (说明 18 +
 * 画布 145 + 边距 12) ≈ 780 < 800 → 全部可见。
 */
import { Root, View, Text, Flex, G2D, requestAnimationFrame } from 'kwikui';

// ─────────────────────────────────────────────────
// 通用小工具
// ─────────────────────────────────────────────────

// 五角星（凹多边形 —— ear clipping 剖分能力展示；Canvas 同款画法）
function starPath(g2d, cx, cy, rOut, rIn) {
    g2d.beginPath();
    for (let i = 0; i < 10; ++i) {
        const ang = -Math.PI / 2 + i * Math.PI / 5;         // 顶点起自正上方
        const r = (i % 2 === 0) ? rOut : rIn;               // 奇偶交替内外半径
        const x = cx + r * Math.cos(ang);
        const y = cy + r * Math.sin(ang);
        if (i === 0) g2d.moveTo(x, y); else g2d.lineTo(x, y);
    }
    g2d.closePath();
}

// 箭头（凹多边形：矩形杆 + 三角头一次轮廓画完）
function arrowPath(g2d, x, y, len, h) {
    const m = y;                 // 箭杆中线
    const w = 8;                 // 箭杆厚度
    g2d.beginPath();
    g2d.moveTo(x, m - w / 2);
    g2d.lineTo(x + len - h, m - w / 2);
    g2d.lineTo(x + len - h, m - h / 2);
    g2d.lineTo(x + len, m);      // 箭头尖
    g2d.lineTo(x + len - h, m + h / 2);
    g2d.lineTo(x + len - h, m + w / 2);
    g2d.lineTo(x, m + w / 2);
    g2d.closePath();
}

// 闪电（强凹多边形，检验耳切剖分不自交失真）
function boltPath(g2d, cx, cy) {
    g2d.beginPath();
    g2d.moveTo(cx - 4, cy - 34);
    g2d.lineTo(cx + 14, cy - 34);
    g2d.lineTo(cx + 2, cy - 8);
    g2d.lineTo(cx + 16, cy - 8);
    g2d.lineTo(cx - 10, cy + 34);
    g2d.lineTo(cx - 2, cy + 2);
    g2d.lineTo(cx - 16, cy + 2);
    g2d.closePath();
}

// ═════════════════════════════════════════════════
// ① 基础图形：矩形填充/描边、圆、椭圆
// ═════════════════════════════════════════════════
const c1 = G2D({ id: "g2d-basic", width: 380, height: 145 });
c1.setFillStyle("#E74C3C");  c1.fillRect(15, 15, 100, 60);
c1.setFillStyle("#F39C12");  c1.fillRect(125, 15, 100, 60);
c1.setStrokeStyle("#2ECC71"); c1.setLineWidth(4); c1.strokeRect(15, 85, 100, 45);
c1.setStrokeStyle("#3498DB"); c1.setLineWidth(2); c1.strokeRect(125, 85, 100, 45);
c1.setFillStyle("#1ABC9C");
c1.beginPath(); c1.arc(285, 45, 28, 0, Math.PI * 2, false); c1.fill();
c1.setFillStyle("#8E44AD");
c1.beginPath(); c1.ellipse(285, 110, 42, 18, 0.4, 0, Math.PI * 2, false); c1.fill();

// ═════════════════════════════════════════════════
// ② 描边宽度谱系 + 折线（setLineWidth 1→8）
// ═════════════════════════════════════════════════
const c2 = G2D({ id: "g2d-stroke", width: 380, height: 145 });
c2.setStrokeStyle("#3498DB");
for (let i = 0; i < 4; ++i) {
    c2.setLineWidth(1 + i * 2);
    c2.strokeRect(20 + i * 60, 20, 44, 40);
}
// 折线 polyline（moveTo + lineTo 链）
c2.setStrokeStyle("#E67E22"); c2.setLineWidth(3);
c2.beginPath();
c2.moveTo(20, 100);
c2.lineTo(80, 70);  c2.lineTo(140, 115); c2.lineTo(200, 75);
c2.lineTo(260, 110); c2.lineTo(340, 85);
c2.stroke();
// 圆点标注折点（小圆）
c2.setFillStyle("#E67E22");
const pts = [[20,100],[80,70],[140,115],[200,75],[260,110],[340,85]];
for (const [px, py] of pts) {
    c2.beginPath(); c2.arc(px, py, 3, 0, Math.PI * 2, false); c2.fill();
}

// ═════════════════════════════════════════════════
// ③ 曲线：二次 / 三次贝塞尔 + 波浪（quadraticCurveTo 链）
// ═════════════════════════════════════════════════
const c3 = G2D({ id: "g2d-curve", width: 380, height: 145 });
c3.setStrokeStyle("#2ECC71"); c3.setLineWidth(3);
c3.beginPath();
c3.moveTo(20, 45);
c3.quadraticCurveTo(80, 5, 140, 45);
c3.stroke();
c3.setStrokeStyle("#3498DB");
c3.beginPath();
c3.moveTo(170, 45);
c3.bezierCurveTo(210, -5, 250, 85, 290, 45);
c3.stroke();
// 正弦波（quadratic 链近似，控制点在峰谷之间）
c3.setStrokeStyle("#9B59B6"); c3.setLineWidth(2.5);
c3.beginPath();
c3.moveTo(20, 105);
for (let i = 0; i < 8; ++i) {
    const x0 = 20 + i * 42, x1 = x0 + 21, x2 = x0 + 42;
    const dir = (i % 2 === 0) ? 1 : -1;
    c3.quadraticCurveTo(x1, 105 + dir * 22, x2, 105);
}
c3.stroke();
// 曲线控制点指示（灰点）
c3.setFillStyle("#95A5A6");
c3.beginPath(); c3.arc(80, 5, 3, 0, Math.PI * 2, false); c3.fill();
c3.beginPath(); c3.arc(210, 5, 3, 0, Math.PI * 2, false); c3.fill();
c3.beginPath(); c3.arc(250, 85, 3, 0, Math.PI * 2, false); c3.fill();

// ═════════════════════════════════════════════════
// ④ 圆弧与扇形：arc 局部弧 / 饼图扇形 / 圆环弧段
// ═════════════════════════════════════════════════
const c4 = G2D({ id: "g2d-arc", width: 380, height: 145 });
// 饼图扇形 ×3（moveTo 圆心 → arc → closePath，隐式闭合填充）
const slices = [
    [0,                   Math.PI * 0.9,  "#E74C3C"],
    [Math.PI * 0.9,       Math.PI * 1.7,  "#F39C12"],
    [Math.PI * 1.7,       Math.PI * 2.0,  "#2ECC71"],
];
for (const [a0, a1, col] of slices) {
    c4.setFillStyle(col);
    c4.beginPath();
    c4.moveTo(90, 72);
    c4.arc(90, 72, 52, a0, a1, false);
    c4.closePath();
    c4.fill();
}
// 局部弧描边（仪表盘刻度感）
c4.setStrokeStyle("#3498DB"); c4.setLineWidth(6);
c4.beginPath(); c4.arc(240, 72, 42, Math.PI * 0.75, Math.PI * 1.75, false); c4.stroke();
// 多段圆点弧
c4.setStrokeStyle("#1ABC9C"); c4.setLineWidth(3);
for (let i = 0; i < 5; ++i) {
    c4.beginPath();
    c4.arc(240, 72, 52, Math.PI * (0.75 + i * 0.22), Math.PI * (0.85 + i * 0.22), false);
    c4.stroke();
}
// 拱形半圆
c4.setStrokeStyle("#E67E22"); c4.setLineWidth(4);
c4.beginPath(); c4.arc(330, 110, 26, Math.PI, Math.PI * 2, false); c4.stroke();

// ═════════════════════════════════════════════════
// ⑤ 凹多边形填充（ear clipping 剖分）：五角星 / 箭头 / 闪电
//    形心扇剖分对此类形状会失真，现在一次 fill 正确成型
// ═════════════════════════════════════════════════
const c5 = G2D({ id: "g2d-concave", width: 380, height: 145 });
c5.setFillStyle("#F1C40F");
starPath(c5, 70, 72, 44, 18); c5.fill();
c5.setFillStyle("#3498DB");
arrowPath(c5, 130, 72, 90, 36); c5.fill();
c5.setFillStyle("#F39C12");
boltPath(c5, 265, 70); c5.fill();
// 凹多边形描边对比（同一形状只描边不填充）
c5.setStrokeStyle("#E74C3C"); c5.setLineWidth(2);
boltPath(c5, 330, 70); c5.stroke();

// ═════════════════════════════════════════════════
// ⑥ 渐变画刷：线性（水平/垂直/对角）+ 径向
//    （fillRect 走真渐变；路径填充取起点色兜底）
// ═════════════════════════════════════════════════
const c6 = G2D({ id: "g2d-grad", width: 380, height: 145 });
c6.setFillStyle(c6.createLinearGradient(15, 0, 115, 0, "#3498DB", "#E74C3C"));
c6.fillRect(15, 15, 100, 50);                       // 水平
c6.setFillStyle(c6.createLinearGradient(0, 80, 0, 130, "#2ECC71", "#1ABC9C"));
c6.fillRect(15, 80, 100, 50);                       // 垂直
c6.setFillStyle(c6.createLinearGradient(125, 15, 245, 130, "#F39C12", "#9B59B6"));
c6.fillRect(125, 15, 120, 115);                     // 对角
c6.setFillStyle(c6.createRadialGradient(310, 72, 10, 310, 72, 58, "#FCE38A", "#F38181"));
c6.beginPath(); c6.arc(310, 72, 58, 0, Math.PI * 2, false); c6.fill();   // 径向→起点色兜底（圆填充）

// ═════════════════════════════════════════════════
// ⑦ 变换：translate / rotate / scale（save/restore 弹压）
// ═════════════════════════════════════════════════
const c7 = G2D({ id: "g2d-transform", width: 380, height: 145 });
// 旋转矩形（绕自身中心）
c7.save();
c7.translate(70, 72); c7.rotate(Math.PI / 6);
c7.setFillStyle("#27AE60"); c7.fillRect(-40, -16, 80, 32);
c7.restore();
// 旋转 + 缩放组合（椭圆化方块 → 变换复合）
c7.save();
c7.translate(180, 72); c7.rotate(-Math.PI / 4); c7.scale(1.4, 0.6);
c7.setFillStyle("#8E44AD"); c7.fillRect(-30, -30, 60, 60);
c7.restore();
// 风车叶：同一矩形绕中心旋转 4 次叠出（translate 到轴心）
for (let i = 0; i < 4; ++i) {
    c7.save();
    c7.translate(300, 72);
    c7.rotate(i * Math.PI / 4);
    c7.setFillStyle(i % 2 === 0 ? "#E74C3C" : "#F39C12");
    c7.fillRect(0, -8, 46, 16);                     // 从轴心伸出的叶片
    c7.restore();
}
c7.setFillStyle("#2C3E50");
c7.beginPath(); c7.arc(300, 72, 5, 0, Math.PI * 2, false); c7.fill();

// ═════════════════════════════════════════════════
// ⑧ 文本：fillText 多字号 / 多颜色（TextRenderPipeline 同管线）
// ═════════════════════════════════════════════════
const c8 = G2D({ id: "g2d-text", width: 380, height: 145 });
c8.setFillStyle("#2C3E50");
c8.fillText("fillText 文本绘制", 15, 15, 18);
c8.setFillStyle("#E74C3C");
c8.fillText("18px 红色标题", 15, 45, 18);
c8.setFillStyle("#3498DB");
c8.fillText("14px 缺省字号正文", 15, 75);            // 第 4 参缺省 = 14
c8.setFillStyle("#7F8C8D");
c8.fillText("11px 辅助说明 G2D text pipeline", 15, 100, 11);
// 中文 + 英文混排
c8.setFillStyle("#27AE60");
c8.fillText("中英混排 Mixed 混排 Text", 200, 45, 13);
// 文本 + 图形组合（徽标：圆 + 字）
c8.setFillStyle("#F39C12");
c8.beginPath(); c8.arc(230, 100, 22, 0, Math.PI * 2, false); c8.fill();
c8.setFillStyle("#FFFFFF");
c8.fillText("G2D", 216, 94, 13);

// ═════════════════════════════════════════════════
// ⑨ 透明度叠加：setGlobalAlpha + save/restore
// ═════════════════════════════════════════════════
const c9 = G2D({ id: "g2d-alpha", width: 380, height: 145 });
// 经典三原色圆叠加（alpha 0.55，重叠区混色）
c9.setGlobalAlpha(0.55);
c9.setFillStyle("#E74C3C");
c9.beginPath(); c9.arc(90, 55, 40, 0, Math.PI * 2, false); c9.fill();
c9.setFillStyle("#2ECC71");
c9.beginPath(); c9.arc(130, 55, 40, 0, Math.PI * 2, false); c9.fill();
c9.setFillStyle("#3498DB");
c9.beginPath(); c9.arc(110, 90, 40, 0, Math.PI * 2, false); c9.fill();
c9.setGlobalAlpha(1.0);                             // 恢复不透明
// alpha=1.0 的实心圆
c9.setFillStyle("#2C3E50");
c9.beginPath(); c9.arc(230, 72, 20, 0, Math.PI * 2, false); c9.fill();
// 阶梯透明度条
for (let i = 0; i < 6; ++i) {
    c9.save();
    c9.setGlobalAlpha(0.15 + i * 0.17);
    c9.setFillStyle("#E74C3C");
    c9.fillRect(270, 20 + i * 20, 90, 14);
    c9.restore();
}

// ═════════════════════════════════════════════════
// ⑩ clearRect 挖空：镂空矩形 / 窗洞 / 底片效果
// ═════════════════════════════════════════════════
const c10 = G2D({ id: "g2d-clear", width: 380, height: 145 });
c10.setFillStyle("#16A085");
c10.fillRect(15, 15, 100, 100);
c10.clearRect(35, 35, 60, 60);                      // 中央挖方孔
c10.setFillStyle("#2980B9");
c10.fillRect(125, 15, 100, 100);
c10.clearRect(140, 30, 30, 30);                     // 四窗洞
c10.clearRect(180, 30, 30, 30);
c10.clearRect(140, 75, 30, 30);
c10.clearRect(180, 75, 30, 30);
// 相机底片：全画横条 + clearRect 齿孔
c10.setFillStyle("#34495E");
c10.fillRect(235, 30, 130, 70);
for (let i = 0; i < 6; ++i) c10.clearRect(242 + i * 20, 38, 12, 10);   // 上排齿孔
for (let i = 0; i < 6; ++i) c10.clearRect(242 + i * 20, 82, 12, 10);   // 下排齿孔
c10.setFillStyle("#F1C40F");
c10.fillRect(255, 52, 90, 26);                      // 片身

// ═════════════════════════════════════════════════
// ⑪ 场景综合：房子 + 树 + 太阳（多 API 组合）
// ═════════════════════════════════════════════════
const c11 = G2D({ id: "g2d-scene", width: 380, height: 145 });
c11.setFillStyle("#AEDFF7");                        // 天空
c11.fillRect(0, 0, 380, 145);
c11.setFillStyle("#7EC850");                        // 草地
c11.fillRect(0, 105, 380, 40);
// 太阳（径向兜底色圆 + 光线短线）
c11.setFillStyle("#FDB813");
c11.beginPath(); c11.arc(320, 38, 24, 0, Math.PI * 2, false); c11.fill();
c11.setStrokeStyle("#FDB813"); c11.setLineWidth(2);
for (let i = 0; i < 8; ++i) {
    const a = i * Math.PI / 4;
    c11.beginPath();
    c11.moveTo(320 + 30 * Math.cos(a), 38 + 30 * Math.sin(a));
    c11.lineTo(320 + 40 * Math.cos(a), 38 + 40 * Math.sin(a));
    c11.stroke();
}
// 房子：墙体 + 三角屋顶 + 门 + 窗
c11.setFillStyle("#E8DAC8");                        // 墙
c11.fillRect(50, 55, 110, 60);
c11.setFillStyle("#C0392B");                        // 屋顶（三角路径填充）
c11.beginPath();
c11.moveTo(40, 55); c11.lineTo(105, 20); c11.lineTo(170, 55);
c11.closePath(); c11.fill();
c11.setFillStyle("#8B5A2B");                        // 门
c11.fillRect(95, 80, 24, 35);
c11.setFillStyle("#F1C40F");                        // 门把手
c11.beginPath(); c11.arc(114, 98, 2.5, 0, Math.PI * 2, false); c11.fill();
c11.setFillStyle("#5DADE2");                        // 窗 + 十字框
c11.fillRect(62, 65, 24, 20); c11.fillRect(124, 65, 24, 20);
c11.setStrokeStyle("#FFFFFF"); c11.setLineWidth(1.5);
c11.strokeRect(62, 65, 24, 20); c11.strokeRect(124, 65, 24, 20);
// 树 ×2：树干矩形 + 树冠圆
c11.setFillStyle("#8B5A2B");
c11.fillRect(220, 80, 10, 30);
c11.setFillStyle("#27AE60");
c11.beginPath(); c11.arc(225, 68, 22, 0, Math.PI * 2, false); c11.fill();
c11.setFillStyle("#8B5A2B");
c11.fillRect(260, 88, 8, 22);
c11.setFillStyle("#2ECC71");
c11.beginPath(); c11.arc(264, 78, 17, 0, Math.PI * 2, false); c11.fill();
// 烟囱（屋顶右上，旋转矩形演示倾斜贴合省略——保持直观矩形）
c11.setFillStyle("#7F8C8D");
c11.fillRect(140, 30, 10, 18);

// ═════════════════════════════════════════════════
// ⑫ 动画：requestAnimationFrame + reset() 逐帧重录
//    旋转五角星（凹多边形逐帧重剖）+ 轨道点 + 脉冲圆
// ═════════════════════════════════════════════════
const c12 = G2D({ id: "g2d-anim", width: 380, height: 145 });
let animT = 0;
function drawAnimFrame() {
    c12.reset();                                    // 清命令列表 + 样式，重录本帧
    c12.setFillStyle("#10151E");
    c12.fillRect(0, 0, 380, 145);                   // 深色底
    // 轨道环
    c12.setStrokeStyle("#2A3444"); c12.setLineWidth(1);
    c12.beginPath(); c12.arc(190, 72, 50, 0, Math.PI * 2, false); c12.stroke();
    // 旋转五角星（左侧，凹多边形逐帧重推 ear clipping）
    c12.save();
    c12.translate(80, 72);
    c12.rotate(animT * 0.6);
    c12.setFillStyle("#3498DB");
    starPath(c12, 0, 0, 34, 14);
    c12.fill();
    c12.restore();
    // 轨道运行点（右侧）
    const px = 190 + 50 * Math.cos(animT * 1.2);
    const py = 72 + 50 * Math.sin(animT * 1.2);
    c12.setFillStyle("#F39C12");
    c12.beginPath(); c12.arc(px, py, 6, 0, Math.PI * 2, false); c12.fill();
    // 脉冲圆（呼吸）
    const pr = 18 + 7 * Math.sin(animT * 2.0);
    c12.setGlobalAlpha(0.7);
    c12.setFillStyle("#2ECC71");
    c12.beginPath(); c12.arc(300, 72, pr, 0, Math.PI * 2, false); c12.fill();
    c12.setGlobalAlpha(1.0);
    // 帧标（文本随角度走的数值读数）
    c12.setFillStyle("#7F8C8D");
    c12.fillText("t=" + animT.toFixed(1), 15, 8, 11);
}
function animLoop() {
    animT += 0.05;
    drawAnimFrame();
    requestAnimationFrame(animLoop);
}
animLoop();

// ═════════════════════════════════════════════════
// 页面组装：标题 + 4 行 × 3 画布（带说明文字）
// ═════════════════════════════════════════════════
function caption(t) {
    return Text({ text: t, fontSize: 12, color: [90, 99, 112, 255], margin: [0, 0, 3, 0] });
}
// caption 在画布上方 → cell = [caption, canvas] 垂直排
function cell(cap, g) {
    return View({ width: 380 }, [cap, g]);
}
function row3(capA, gA, capB, gB, capC, gC) {
    return Flex({ direction: "row", gap: 24, margin: [0, 0, 10, 0] }, [
        cell(capA, gA), cell(capB, gB), cell(capC, gC),
    ]);
}

export default () => Root(View({ width: 1280, height: 800, background: [245, 245, 245, 255], padding: [24, 24, 24, 24] }, [
    Text({ text: "G2D 2D 绘制综合示例（12 画布：基础 / 路径 / 凹多边形 ear clipping / 渐变 / 变换 / 文本 / 透明度 / 挖空 / 场景 / 动画）", fontSize: 15, margin: [0, 0, 10, 0] }),

    row3(
        caption("① 基础图形 fillRect/strokeRect/arc/ellipse"), c1,
        caption("② 描边宽度谱系 + 折线 polyline"), c2,
        caption("③ 贝塞尔曲线 + 正弦波"), c3),
    row3(
        caption("④ 圆弧与扇形（饼图/仪表弧/拱形）"), c4,
        caption("⑤ 凹多边形填充（ear clipping）：星/箭头/闪电"), c5,
        caption("⑥ 渐变：线性水平/垂直/对角 + 径向"), c6),
    row3(
        caption("⑦ 变换 translate/rotate/scale + 风车"), c7,
        caption("⑧ 文本 fillText 多字号/混排/徽标"), c8,
        caption("⑨ 透明度叠加 setGlobalAlpha"), c9),
    row3(
        caption("⑩ clearRect 挖空：镂空/窗洞/底片齿孔"), c10,
        caption("⑪ 场景综合：房子+树+太阳"), c11,
        caption("⑫ 动画 rAF：旋转星/轨道/脉冲（逐帧 reset 重录）"), c12),
]));
