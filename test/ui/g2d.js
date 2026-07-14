/**
 * R2D 2D 绘制综合示例
 *
 * 覆盖所有 Phase 1 API：
 *   fillRect / strokeRect / clearRect
 *   beginPath / moveTo / lineTo / quadraticCurveTo / bezierCurveTo
 *   arc / ellipse / closePath
 *   fill / stroke
 *   save / restore
 *   setFillStyle / setStrokeStyle / setLineWidth / setGlobalAlpha
 */
import { View, G2D, Root } from 'kwikui';

// ─────────────────────────────────────────────────
// 创建 R2D 实例（画布 400×300 逻辑像素）
// ─────────────────────────────────────────────────
const g2d = G2D({ id: "demo", width: 400, height: 300 });

// ═════════════════════════════════════════════════
//  1. fillRect — 填充矩形
//     setFillStyle — 设置填充颜色（支持 #rgb / #rrggbb / rgba）
// ═════════════════════════════════════════════════

// 红色实心矩形（左下角）
g2d.setFillStyle("#E74C3C");
g2d.fillRect(10, 10, 120, 80);

// 橙色实心矩形（紧邻右侧）
g2d.setFillStyle("#F39C12");
g2d.fillRect(140, 10, 120, 80);

// 黄色实心矩形（使用 rgba 格式）
g2d.setFillStyle("rgba(241,196,15,1)");
g2d.fillRect(250, 10, 120, 80);   // 逐步右移


// ═════════════════════════════════════════════════
//  2. strokeRect — 描边矩形（仅边框）
//     setStrokeStyle → 描边颜色
//     setLineWidth   → 线宽
// ═════════════════════════════════════════════════

// 绿色粗边框矩形
g2d.setStrokeStyle("#2ECC71");
g2d.setLineWidth(4);
g2d.strokeRect(10, 110, 120, 80);

// 蓝色中等边框矩形
g2d.setStrokeStyle("#3498DB");
g2d.setLineWidth(2);
g2d.strokeRect(140, 110, 120, 80);

// 紫色细边框矩形
g2d.setStrokeStyle("#9B59B6");
g2d.setLineWidth(1);
g2d.strokeRect(270, 110, 120, 80);

// ═════════════════════════════════════════════════
//  3. 直线路径 — beginPath / moveTo / lineTo / closePath
//     fill → 填充路径区域
// ═════════════════════════════════════════════════

// 红色填充三角形
g2d.setFillStyle("#E74C3C");
g2d.beginPath();
g2d.moveTo(20, 220);       // 起点
g2d.lineTo(90, 280);       // 连线到右下
g2d.lineTo(20, 280);       // 连线到左下
g2d.closePath();           // 闭合路径（回到起点）
g2d.fill();                // 填充路径内部

// ═════════════════════════════════════════════════
//  4. 二次贝塞尔曲线 — quadraticCurveTo / stroke
// ═════════════════════════════════════════════════

// 绿色开口向上的抛物线
g2d.setStrokeStyle("#2ECC71");
g2d.setLineWidth(3);
g2d.beginPath();
g2d.moveTo(130, 280);             // 起点（左下）
g2d.quadraticCurveTo(180, 220,    // 控制点（顶点处）
                      230, 280);  // 结束点（右下）
g2d.stroke();

// ═════════════════════════════════════════════════
//  5. 三次贝塞尔曲线 — bezierCurveTo / stroke
// ═════════════════════════════════════════════════

// 蓝色 S 形曲线
g2d.setStrokeStyle("#3498DB");
g2d.setLineWidth(3);
g2d.beginPath();
g2d.moveTo(260, 280);
g2d.bezierCurveTo(290, 210,   // 控制点 1（上方，拉出右弯）
                   330, 310,   // 控制点 2（下方，拉回左弯）
                   350, 260);  // 结束点（右上方，形成 S 出口）
g2d.stroke();

// ═════════════════════════════════════════════════
//  6. 圆弧 — arc 完整圆 (fill) + 半圆弧 (stroke)
//     arc(cx, cy, r, startAngle, endAngle, ccw)
// ═════════════════════════════════════════════════

// 青色填充圆形
g2d.setFillStyle("#1ABC9C");
g2d.beginPath();
g2d.arc(280, 210, 30,        // 圆心 (280,210), 半径 30
        0, Math.PI * 2,       // 0 → 2π（完整圆）
        false);
g2d.fill();

// 橙色描边半圆弧（拱形）
g2d.setStrokeStyle("#E67E22");
g2d.setLineWidth(4);
g2d.beginPath();
g2d.arc(340, 240, 28,        // 圆心 (340,240), 半径 28
        0, Math.PI,           // 0 → π（上半弧）
        false);
g2d.stroke();

// ═════════════════════════════════════════════════
//  7. 椭圆 — ellipse / fill
//     ellipse(cx, cy, rx, ry, rotation, startAngle, endAngle, ccw)
// ═════════════════════════════════════════════════

// 紫色填充椭圆（旋转 30°）
g2d.setFillStyle("#8E44AD");
g2d.beginPath();
g2d.ellipse(80, 210,         // 中心 (80,210)
            45, 20,           // 半长轴 45, 半短轴 20
            0.5,              // 旋转 0.5 弧度 ≈ 30°
            0, Math.PI * 2,   // 完整椭圆
            false);
g2d.fill();

// ═════════════════════════════════════════════════
//  8. save / restore + globalAlpha
//     save → 保存当前样式状态入栈
//     restore → 从栈顶恢复样式状态
//     setGlobalAlpha → 全局透明度 (0.0 ~ 1.0)
// ═════════════════════════════════════════════════

// 保存当前状态（fillStyle=紫色, strokeStyle=橙色, lineWidth=4, alpha=1.0）
g2d.save();

// 降低透明度绘制两个半透明矩形（相互叠加）
g2d.setGlobalAlpha(0.4);
g2d.setFillStyle("#E74C3C");
g2d.fillRect(150, 200, 60, 60);
g2d.setFillStyle("#3498DB");
g2d.fillRect(180, 220, 60, 60);

// 恢复之前的状态（fillStyle 恢复为紫色，alpha 恢复为 1.0）
g2d.restore();

// 用恢复后的紫色画一个小圆点确认 restore 生效
g2d.beginPath();
g2d.arc(380, 60, 15, 0, Math.PI * 2, false);
g2d.fill();

// ═════════════════════════════════════════════════
//  9. clearRect — 清除矩形区域（挖空效果）
//     清除区域内的所有绘制内容变为透明
// ═════════════════════════════════════════════════

// 从第一个红色矩形中挖掉一块
g2d.clearRect(30, 20, 60, 50);

// 从黄色矩形中挖掉一块
g2d.clearRect(290, 30, 80, 40);

// =================================================
// 导出：放入 Root 容器，20px 内边距
// =================================================
export default Root(
    View({ padding: 20, width: 400, height: 400 }, [g2d])
);