import { View, Text, Dropdown, Button, getProp, setProp, Flex} from 'kwikui';

export default View({
    width: 800,
    height: 600,
    background: "#f0f2f5",
    padding: 24
}, [
    Text({
        text: "Dropdown 下拉选择测试",
        fontSize: 20,
        color: "#1e293b",
        fontWeight: "bold",
        margin: [0, 0, 24, 0]
    }),

    // ── 下拉框 1 ──
    Text({ text: "城市选择", fontSize: 13, color: "#64748b", margin: [0, 0, 6, 0] }),
    Dropdown({
        id: "cityDropdown",
        placeholder: "请选择城市",
        items: ["北京", "上海", "广州", "深圳", "杭州", "成都", "武汉"],
        width: 280,
        fontSize: 14,
        borderWidth: 1,
        borderColor: "#82b1eb",
        borderRadius: 6,
        padding: [5, 8],
        margin: [0, 0, 20, 0],
        onChange: (e) => console.log("[城市] 选中:", e.value, "索引:", e.index)
    }),

    // ── 下拉框 2 ──
    Text({ text: "尺寸选择", fontSize: 13, color: "#64748b", margin: [0, 0, 6, 0] }),
    Dropdown({
        id: "sizeDropdown",
        placeholder: "请选择尺寸",
        items: ["Small", "Medium", "Large", "X-Large"],
        width: 240,
        fontSize: 14,
        borderWidth: 1,
        borderColor: "#4a525c",
        borderRadius: 6,
        padding: [8, 12],
        margin: [0, 0, 24, 0],
        onChange: (e) => console.log("[尺寸] 选中:", e.value, "索引:", e.index)
    }),

    // ── 按钮行 ──
    Flex({ direction: "row", gap: 12, margin: 20}, [
        Button({
            text: "获取城市选中值",
            width: 140,
            height: 36,
            borderRadius: 6,
            onClick: () => {
                const val = getProp("cityDropdown", "value");
                const idx = getProp("cityDropdown", "index");
                console.log("城市选中:", val, "(index:", idx, ")");
            }
        }),
        Button({
            text: "获取尺寸选中值",
            width: 140,
            height: 36,
            borderRadius: 6,
            onClick: () => {
                console.log("尺寸选中:", getProp("sizeDropdown", "value"));
            }
        }),
    ]),
]);