import { Root, View } from 'kwikui';

export default Root(
    View({ background: "#f5f5f5", padding: 40 }, [
        // ① 水平渐变（90°=向右）
        View({ x: 40, y: 40, width: 200, height: 80, borderRadius: 8,
               gradient: "linear 90 #ff6b6b #ffd93d" }),
        // ② 垂直渐变（180°=向下，默认）
        View({ x: 280, y: 40, width: 200, height: 80, borderRadius: 8,
               gradient: "linear 180 #4facfe #00f2fe" }),
        // ③ 斜向渐变（45°）+ 大圆角
        View({ x: 40, y: 160, width: 200, height: 100, borderRadius: 16,
               gradient: "linear 45 #43e97b #38f9d7" }),
        // ④ 径向渐变（中心→边缘 cover）
        View({ x: 280, y: 160, width: 200, height: 100, borderRadius: 12,
               gradient: "radial #f093fb #f5576c" }),
        // ⑤ 渐变 + 边框 + 旋转组合
        View({ x: 40, y: 300, width: 180, height: 70, borderRadius: 10,
               borderWidth: 2, borderColor: "#333",
               transform: "0,0,15,1",
               gradient: "linear 90 #a18cd1 #fbc2eb" }),
        // ⑥ 径向渐变 + 按钮态背景
        View({ x: 280, y: 300, width: 200, height: 120, borderRadius: 8,
               gradient: "radial #ffecd2 #fcb69f" }),
    ])
);