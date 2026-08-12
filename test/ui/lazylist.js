import { View, Text, Root, LazyList } from 'kwikui';

// LazyList 双模式虚拟化滚动列表 demo
// 固定模式（itemHeight）与可变模式（不传 itemHeight，estimatedItemSize 兜底）
const rows = [];
for (let i = 0; i < 1000; i++) rows.push(i);

export default () => Root(
    View({ id: "root", width: 1280, height: 800, background: "#f5f5f5", padding: 20 }, [
        // ── 固定模式：itemHeight=44，divider 分割线，header/footer 固定 ──
        LazyList({
            id: "fixed", width: 480, height: 620, x: 20, y: 20,
            background: "#ffffff", borderRadius: 8,
            direction: "vertical",
            itemHeight: 44,
            dividerHeight: 1, dividerColor: "#eeeeee",
            header: Text({ text: "固定高度列表 (1000 项)", height: 40, fontSize: 15, fontWeight: "bold" }),
            footer: Text({ text: "—— 到底了 ——", height: 36, fontSize: 12, textColor: "#999999" }),
            items: rows,
            itemBuilder: (item, index) => {
                return View({
                    id: "row-" + index, height: 44,
                    background: index % 2 ? "#fafafa" : "#ffffff"
                }, [
                    Text({ text: "第 " + index + " 行 · 数据 " + item, x: 12, y: 14, fontSize: 14 })
                ]);
            },
        }),

        // ── 可变模式：行高随 index 变化（实测缓存，滚过即收敛）──
        LazyList({
            id: "variable", width: 480, height: 620, x: 560, y: 20,
            background: "#ffffff", borderRadius: 8,
            direction: "vertical",
            estimatedItemSize: 48,
            items: rows,
            itemBuilder: (item, index) => {
                const h = 40 + (index % 5) * 12;    // 40 / 52 / 64 / 76 / 88 交替
                return View({ width: 480, height: h, background: "#f0f8ff" }, [
                    Text({ text: "可变行 " + index + " · 高 " + h, x: 12, y: h / 2 - 9, fontSize: 14 })
                ]);
            },
        }),
    ])
);