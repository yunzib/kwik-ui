/**
 * G3D 3D 绘制综合示例
 *
 * 覆盖 Phase 1 API：
 *   loadModel / clear / setColor / autoRotate / rotateTo
 *   addBox / addSphere / addPlane
 *
 * 交互：鼠标按住拖拽 → Pan → 轨道相机 yaw/pitch（拖拽时暂停 autoRotate）
 */
import { View, G3D, Root } from 'kwikui';

// ═════════════════════════════════════════════════
//  1. glTF 模型加载示例（cube.glb，逐面法线）
//     路径与 image.js 约定一致，相对运行目录 (build/test)
// ═════════════════════════════════════════════════
const g3d = G3D({
    id: "demo",
    width: 400,
    height: 300,
    background: "#1a1a2e",
});
g3d.loadModel("../../test/cube.glb");
g3d.setColor("#4fc3f7");   // 浅蓝
g3d.autoRotate(true);      // 自动旋转

// ═════════════════════════════════════════════════
//  2. 内建基本体示例（不加载模型）
//     addBox(size) / addSphere(radius, slices, stacks)
//     addPlane(w, d)
// ═════════════════════════════════════════════════
const prim = G3D({
    id: "prims",
    width: 400,
    height: 300,
    background: "#1a1a2e",
});
prim.setColor("#ffb74d");               // 橙色
prim.addPlane(3.0, 3.0);                // 地面 (原点)
prim.addBox(1.4, -1.5, 0, 0);           // 立方体 (左侧, 与球分离)
prim.addSphere(0.5, 24, 12, 1.5, 0, 0); // 球 (右侧, 不再被箱体遮挡)
prim.rotateTo(0.5, 0.4);                // 绝对轨道角 (弧度)
prim.autoRotate(false);
prim.showAxes(true);                     // 坐标轴 (红 X / 绿 Y / 蓝 Z)

// ═════════════════════════════════════════════════
// 导出：放入 Root 容器，左右并排
// ═════════════════════════════════════════════════
export default Root(
    View({ padding: 20, width: 860, height: 400, flexDirection: "row", gap: 20 }, [
        g3d,
        prim
    ])
);