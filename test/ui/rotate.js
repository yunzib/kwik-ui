import { Root, View } from 'kwikui';

/**
 * rotate 验证示例 — GPU transform（rotate + 旋转圆角裁剪）
 * 运行：example.exe rotate
 * transform 格式："tx,ty,rot,scale"（rot 单位：度，绕中心）
 */
export default Root(
    View({ background: "#f5f5f5", padding: 40 }, [
        // ① 旋转 45° 的圆角矩形（验证填充/描边旋转）
        View({
            x: 60, y: 60, width: 120, height: 60,
            background: "#4CAF50", borderRadius: 12,
            borderWidth: 2, borderColor: "#2E7D32",
            transform: "0,0,45,1"
        }),

        // ② 旋转 30° 的圆角卡片 + 子元素（验证旋转裁剪）
        View({
            x: 60, y: 200, width: 160, height: 100,
            background: "#2196F3", borderRadius: 16,
            transform: "0,0,30,1"
        }, [
            View({ width: 80, height: 40, background: "#FFEB3B", borderRadius: 8, margin: [20, 20, 0, 0] }),
        ]),

        // ③ 旋转 90° 的指针（细长矩形）
        View({
            x: 300, y: 100, width: 100, height: 10,
            background: "#FF5722", borderRadius: 5,
            transform: "0,0,90,1"
        }),

        // ④ 负角度 -20° + 缩放 1.2（验证 rotate+scale 复合）
        View({
            x: 300, y: 240, width: 120, height: 60,
            background: "#9C27B0", borderRadius: 8,
            transform: "0,0,-20,1.2"
        }),

        // ④ 负角度 135° + 缩放 1.2（验证 rotate+scale 复合）
        View({
            x: 200, y: 350, width: 120, height: 60,
            background: "#9C27B0", borderRadius: 8,
            transform: "0,0,105,1.2"
        }),
    ])
);