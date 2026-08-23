// AURORA 车机主题：深色玻璃拟态 + 冷暖撞色
import { theme } from 'kwikui';
export default theme({
    mode: "dark",
    colors: {
        primary: "#FF6B35",       // 暖橙（主色/accent）
        onPrimary: "#FFFFFF",
        surface: "#0a0c14",       // 深藏青背景
        onSurface: "#FFFFFF",
        surfaceVariant: "#171a26",// 卡片底
        onSurfaceVariant: "#8A93A6",
        outline: "#2A3040",
    },
    shape: { borderRadius: 14 },
});