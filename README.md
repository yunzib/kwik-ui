<div align="center">
<h1>kwik-ui(c++ 声明式UI库)</h1>
</div>

<p align="center">
<img alt="" src="https://img.shields.io/badge/release-v0.0.0-brightgreen" style="display: inline-block;" />
<img alt="" src="https://img.shields.io/badge/c++-26-brightgreen" style="display: inline-block;" />
<img alt="" src="https://img.shields.io/badge/js engine-quickjs ng-brightgreen" style="display: inline-block;" />
</p>

# 1. 项目描述
基于 C++26 Modules、QuickJS 与 Vulkan 的声明式跨平台 UI 框架。Vulkan GPU 硬件加速渲染，QuickJS 驱动 JS 声明组件树，实现高性能、低延迟的原生 UI 体验。低开销 C++ 内核 + 灵活 JS 逻辑，适用于嵌入式 Linux 及跨平台应用开发。

# 2. 开发环境
- IDE: VSCODE
    - 插件： clangd, CMake, CMake Tools, opencode
- 操作系统： Windows11
- 编译器： llvm-mingw-20260421-ucrt-x86_64
- 构建系统： cmake 4.3.2
- 构建工具： ninja 1.13.2

# 3. 项目效果展示
## 3.1 代码示例
```
import { View, Text, Button, State } from 'kwikui';
const state = new State({ count: 0 });
export default View({
    width: 800,
    height: 600,
    background: "#f0f0f0",
    padding: 30
}, [
    // ── 标题 ──────────────────────────────
    Text({
        text: "Event Handling Demo",
        fontSize: 24,
        fontWeight: "bold",
        color: "#333"
    }),
    // ── 点击按钮 ──────────────────────────
    Button({
        text: "Click Me",
        width: 180,
        height: 50,
        // background: "#4CAF50",
        borderRadius: 8,
        onClick: function(event) {
            state.count++;
            console.log("[Click] button clicked " + state.count
                + " times, pos=(" + event.x.toFixed(0)
                + "," + event.y.toFixed(0) + ")");
        }
    }),
    // ── 悬停区域 ──────────────────────────
    View({
        width: 350,
        height: 100,
        background: "#2196F3",
        borderRadius: 8,
        onHoverEnter: function() {
            console.log("[Hover] Enter blue box");
        },
        onHoverLeave: function() {
            console.log("[Hover] Leave blue box");
        }
    }),
    // ── 长按区域 ──────────────────────────
    View({
        width: 350,
        height: 100,
        background: "#FF9800",
        borderRadius: 8,
        onLongPress: function(event) {
            console.log("[LongPress] orange box pressed at ("
                + event.x.toFixed(0) + ","
                + event.y.toFixed(0) + ")");
        }
    }),
    // ── 提示文字 ──────────────────────────
    Text({
        text: "Click green  |  Hover blue  |  Long-press orange",
        fontSize: 14,
        color: "#999"
    })
]);
```
```
import { View, Checkbox, Text, State, Button } from 'kwikui';

const form = new State({
    agree: false, news: true, promo: false,
    email: true, sms: false, analytics: false, terms: false,
});

export default () => View({
    width: 800, height: 600, background: "#f5f5f5", padding: 24
}, [
    Text({ text: "用户偏好设置", fontSize: 22, color: "#333333", margin: [0, 0, 20, 0] }),
    Text({ text: "通知", fontSize: 16, color: "#666666", margin: [0, 0, 12, 0] }),
    Checkbox({ text: "接收新闻推送", checked: form.news, onChange: (e) => form.news = e.checked }),
    Checkbox({ text: "接收促销活动通知", checked: form.promo, onChange: (e) => form.promo = e.checked }),
    Checkbox({ text: "接收邮件通知", checked: form.email, onChange: (e) => form.email = e.checked }),
    Checkbox({ text: "接收短信通知", checked: form.sms, onChange: (e) => form.sms = e.checked }),
    Text({ text: "隐私", fontSize: 16, color: "#666666", margin: [0, 0, 12, 0] }),
    Checkbox({ text: "共享使用数据分析", checked: form.analytics, onChange: (e) => form.analytics = e.checked }),
    Checkbox({ text: "同意用户服务条款", checked: form.terms, onChange: (e) => form.terms = e.checked }),
    Text({ text: "法律", fontSize: 16, color: "#666666", margin: [0, 0, 12, 0] }),
    Checkbox({
        text: "已阅读并同意《用户协议》", checked: form.agree,
        checkedColor: "#E53935", checkedFillColor: "#E53935",
        onChange: (e) => form.agree = e.checked
    }),
    Button({
        text: "保存设置", width: 120, height: 44, borderRadius: 8,
        margin: [0, 24, 0, 0],
        onClick: () => {
            console.log("已勾选:");
            if (form.agree) console.log("  - 用户协议");
            if (form.news) console.log("  - 新闻推送");
            if (form.promo) console.log("  - 促销通知");
            if (form.email) console.log("  - 邮件通知");
            if (form.sms) console.log("  - 短信通知");
            if (form.analytics) console.log("  - 数据分析");
            if (form.terms) console.log("  - 服务条款");
        }
    }),
]);
```
## 3.2 效果示例
![alt text](doc/image/examle.png)
- 更多示例可参考:  examples/
- 更多组件相关参考:  doc/1.kwik-ui 组件.md