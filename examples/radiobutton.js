import { RadioGroup, RadioButton, State, View, Text, Button } from 'kwikui';

const form = new State({ size: "Medium" });

const group = RadioGroup({
    name: "size",
    selected: form.size,
    onChange: (e) => { form.size = e.value; }
}, [
    RadioButton({ value: "Small", text: "Small", group: "size" }),
    RadioButton({ value: "Medium", text: "Medium", group: "size" }),
    RadioButton({ value: "Large", text: "Large", group: "size" }),
]);

// 导出根View
export default View({
    width: 800,
    height: 600,
    background: "#f5f5f5",
    borderRadius: 8,
    borderWidth: 1,
    borderColor: "#ddd",
    padding: 20
}, [
    Text({ text: "RadioButton 测试", fontSize: 14 }),
    group,
    Button({
        text: "获取当前项",
        width: 100,
        height: 40,
        // background: "#4CAF50",
        borderRadius: 8,
        onClick: (event) => {
            console.log("on select:" + form.size);
        }
    }),
]);