import { Root, View, Text, Dropdown, Button, Flex, getProp, setProp, State, ref } from 'kwikui';

const form = new State({
    city: "上海",
    size: ""
});

const makeBtn = (label, bgColor, onClick) => Button({
    width: 100, height: 34,
    background: bgColor, borderRadius: 4,
    text: label,
    color: "#ffffff",
    fontSize: 13,
    onClick: onClick,
    margin: [0, 0, 0, 0]
});

export default () => Root(View({
    width: 800,
    height: 600,
    background: "#f0f2f5",
    padding: 24
}, [
    Text({ text: "Dropdown 下拉选择测试（含双向绑定）", fontSize: 20, color: "#1e293b", fontWeight: "bold", margin: [0, 0, 20, 0] }),

    // ════════════════════════════════════════════════════════════════════
    // 一、非受控：城市选择
    // ════════════════════════════════════════════════════════════════════
    Text({ text: "一、城市选择（非受控）", fontSize: 15, fontWeight: "bold", color: "#666", margin: [0, 0, 6, 0] }),
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
        margin: [0, 0, 16, 0],
        onChange: (e) => console.log("[城市] 选中:", e.value, "索引:", e.index)
    }),

    // ════════════════════════════════════════════════════════════════════
    // 二、非受控：尺寸选择
    // ════════════════════════════════════════════════════════════════════
    Text({ text: "二、尺寸选择（非受控）", fontSize: 15, fontWeight: "bold", color: "#666", margin: [0, 0, 6, 0] }),
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
        margin: [0, 0, 16, 0],
        onChange: (e) => console.log("[尺寸] 选中:", e.value, "索引:", e.index)
    }),

    // ════════════════════════════════════════════════════════════════════
    // 操作按钮
    // ════════════════════════════════════════════════════════════════════
    Flex({ direction: "row", gap: 12, margin: [0, 0, 24, 0] }, [
        makeBtn("获取城市", "#4285f4", () => {
            const val = getProp("cityDropdown", "value");
            const idx = getProp("cityDropdown", "index");
            console.log("城市选中:", val, "(index:", idx, ")");
        }),
        makeBtn("获取尺寸", "#4285f4", () => {
            console.log("尺寸选中:", getProp("sizeDropdown", "value"));
        }),
        makeBtn("城市→广州", "#34a853", () => setProp("cityDropdown", "value", "广州")),
        makeBtn("尺寸→Large", "#34a853", () => setProp("sizeDropdown", "value", "Large")),
    ]),

    // ════════════════════════════════════════════════════════════════════
    // 三、双向绑定 ref — 城市
    // ════════════════════════════════════════════════════════════════════
    Text({ text: "三、双向绑定 ref (form.city)", fontSize: 15, fontWeight: "bold", color: "#666", margin: [0, 0, 6, 0] }),
    Dropdown({
        id: "cityRef",
        value: ref(form, "city"),
        placeholder: "请选择城市",
        items: ["北京", "上海", "广州", "深圳", "杭州", "成都", "武汉"],
        width: 280,
        fontSize: 14,
        borderWidth: 1,
        borderColor: "#2196F3",
        borderRadius: 6,
        padding: [5, 8],
        margin: [0, 0, 4, 0],
        onChange: (e) => console.log("[ref城市] 选中:", e.value)
    }),
    Text({ text: `→ State form.city = "${form.city}"`, fontSize: 13, color: "#999", margin: [0, 0, 8, 0] }),
    Flex({ direction: "row", gap: 8, margin: [0, 0, 20, 0] }, [
        makeBtn("getProp", "#34a853", () => console.log("cityRef:", getProp("cityRef", "value"))),
        makeBtn("设→北京", "#4285f4", () => setProp("cityRef", "value", "北京")),
        makeBtn("设→深圳", "#4285f4", () => setProp("cityRef", "value", "深圳")),
        makeBtn("form.city", "#9c27b0", () => console.log("form.city:", form.city)),
        makeBtn("State→成都", "#ff9800", () => { form.city = "成都"; }),
    ]),

    // ════════════════════════════════════════════════════════════════════
    // 四、双向绑定 ref — 尺寸（空值初始）
    // ════════════════════════════════════════════════════════════════════
    Text({ text: "四、双向绑定 ref (form.size，空值初始)", fontSize: 15, fontWeight: "bold", color: "#666", margin: [0, 0, 6, 0] }),
    Dropdown({
        id: "sizeRef",
        value: ref(form, "size"),
        placeholder: "请选择尺寸",
        items: ["Small", "Medium", "Large", "X-Large"],
        width: 240,
        fontSize: 14,
        borderWidth: 1,
        borderColor: "#FF9800",
        borderRadius: 6,
        padding: [8, 12],
        margin: [0, 0, 4, 0],
        onChange: (e) => console.log("[ref尺寸] 选中:", e.value)
    }),
    Text({ text: `→ State form.size = "${form.size}"`, fontSize: 13, color: "#999", margin: [0, 0, 8, 0] }),
    Flex({ direction: "row", gap: 8, margin: [0, 0, 20, 0] }, [
        makeBtn("getProp", "#34a853", () => console.log("sizeRef:", getProp("sizeRef", "value"))),
        makeBtn("设→Medium", "#4285f4", () => setProp("sizeRef", "value", "Medium")),
        makeBtn("设→Large", "#4285f4", () => setProp("sizeRef", "value", "Large")),
        makeBtn("form.size", "#9c27b0", () => console.log("form.size:", form.size)),
    ]),
]));