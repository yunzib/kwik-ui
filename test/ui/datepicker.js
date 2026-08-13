import { Root, View, Text, DateTimePicker, Button, Flex, getProp, setProp, State, ref } from 'kwikui';

const makeBtn = (id, label, onClick) => Button({
    id, width: 120, height: 30, text: label,
    borderRadius: 6, background: [25, 118, 210, 255], textColor: [255, 255, 255, 255],
    fontSize: 13, margin: [0, 8, 0, 0], onClick
});

export default () => Root(View({ width: 800, height: 700, background: [245, 245, 245, 255], padding: [24, 24, 24, 24] }, [
    Text({ text: "DateTimePicker 三个模式", fontSize: 18, margin: [0, 0, 12, 0] }),

    // ── 1) date 模式（未控） ──
    Text({ text: "date 模式", fontSize: 13, margin: [0, 0, 4, 0] }),
    DateTimePicker({
        id: "dp1", mode: "date", placeholder: "请选择生日", width: 240,
        borderColor: [200, 200, 200, 255], borderWidth: 1, borderRadius: 6,
        padding: [8, 12, 8, 12], margin: [0, 0, 16, 0],
        onChange: (e) => console.log("date:", e.value)
    }),

    // ── 2) time 模式（未控） ──
    Text({ text: "time 模式", fontSize: 13, margin: [0, 0, 4, 0] }),
    DateTimePicker({
        id: "tp1", mode: "time", width: 240, margin: [0, 0, 16, 0],
        onChange: (e) => console.log("time:", e.value)
    }),

    // ── 3) datetime 模式（未控） ──
    Text({ text: "datetime 模式", fontSize: 13, margin: [0, 0, 4, 0] }),
    DateTimePicker({
        id: "dtp1", mode: "datetime", width: 240, margin: [0, 0, 16, 0],
        onChange: (e) => console.log("datetime:", e.value)
    }),

    // ── 4) 受控（ref 双向绑定） ──
    Text({ text: "受控（ref）", fontSize: 13, margin: [0, 0, 4, 0] }),
    (() => {
        const form = new State({ date: "2026-08-13" });
        return Flex({ direction: "row", gap: 8, alignItems: "center", margin: [0, 0, 16, 0] }, [
            DateTimePicker({
                id: "dpBound", mode: "date", value: ref(form, "date"), width: 200,
                onChange: (e) => console.log("bound date:", e.value)
            }),
            Text({ text: "当前值: " + form.date, fontSize: 13 }),
            makeBtn("inc", "改 state", () => { form.date = "2025-12-25"; })
        ]);
    })(),

    // ── 5) 命令式 getProp/setProp ──
    Flex({ direction: "row", gap: 8 }, [
        DateTimePicker({ id: "dpCmd1", mode: "datetime", width: 260, margin: [0, 8, 0, 0] }),
        makeBtn("set", "setProp", () => setProp("dpCmd1", "value", "2027-01-01 09:30")),
        makeBtn("get", "getProp", () => console.log("get:", getProp("dpCmd1", "value")))
    ]),
]));