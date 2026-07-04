import * as kwikui from 'kwikui';
import { View, Root, State } from 'kwikui';

console.log("kwikui 模块导出:", Object.keys(kwikui));
const s = new State({ count: 0 });
console.log("State 实例:");
console.log("初始 count = ", s.count);
console.log(typeof s.update);

console.log("加载 Root 示例");


/**
 * KwiK UI 示例
 * Root 为应用入口容器，大小跟随窗口变化
 * Root 透明不渲染内容，子节点自行决定布局
 */
export default Root(
    View({
        background: "#f5f5f5",
        borderRadius: 8,
        borderWidth: 1,
        borderColor: "#ddd",
        padding: 20
    }, [
        // 绿色View
        View({
            width: 100,
            height: 100,
            background: "#4CAF50",
            borderRadius: 4,
            margin: [10, 0, 0, 0]
        }),
        // 蓝色View（带边框）
        View({
            width: 100,
            height: 100,
            background: "#2196F3",
            borderRadius: 12,
            borderWidth: 2,
            borderColor: "#1565C0",
            margin: [10, 0, 0, 0]
        }),

        // 橙色View（带阴影和圆角）
        View({
            width: 100,
            height: 100,
            background: "#FF5722",
            borderRadius: 50,
            shadow: "0 4px 8px rgba(0,0,0,0.3)",
            margin: [10, 0, 0, 0],
            x: 200,
            y: 200
        }),

        View({ width: 200, height: 48, background: "#2196F3", borderRadius: 6, opacity: 1.0 }),
        View({ width: 200, height: 48, background: "#FF0000", borderRadius: 6, opacity: 1.0 }),


        View({ width: 400, height: 300, background: "#FFFFFF", borderRadius: 6, padding: 14, x: 400}, [
            // 纯 View 演示 opacity
            View({ width: 200, height: 48, background: "#2196F3", borderRadius: 6, opacity: 0.6 }),
            View({ width: 200, height: 48, background: "#2196F3", borderRadius: 6, opacity: 0.3 }),
            View({ width: 200, height: 48, background: "#2196F3", borderRadius: 6, opacity: 0.1 }),
            View({ width: 200, height: 48, background: "#2196F3", borderRadius: 6, opacity: 0.0 }),
        ]),
    ])
);
