import { View } from 'kwikui';        // ← JS_NewCModule 已注册, 自动解析
import { Header } from './header.js';  // ← moduleLoader 读文件加载
import { Footer } from './footer.js';  // ← moduleLoader 读文件加载
export default view({ width: 800, height: 600, background: "#fff" }, [
    Header({ title: "KwiK UI" }),
    // ... 内容组件
    Footer({ copyright: "2026" }),
]);