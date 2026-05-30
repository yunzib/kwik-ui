import { View, Checkbox, Text, State, Button } from 'kwikui';
// State 定义在模块顶层 (每次 rebuild 复用同一个实例)
const form = new State({
    agree: false, news: true, promo: false,
    email: true, sms: false, analytics: false, terms: false,
});
// 函数式导出: rebuildTree 时重新调用 → checked 值重新求值
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