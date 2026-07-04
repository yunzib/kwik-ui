/**
 * Event Handling Demo
 *
 * 操作说明:
 *   1. 点击绿色 Button        → 控制台输出点击次数
 *   2. 鼠标移入蓝色区域        → 控制台输出 "Enter blue box"
 *   3. 鼠标移出蓝色区域        → 控制台输出 "Leave blue box"
 *   4. 长按橙色区域 (>0.6秒)   → 控制台输出 "orange box pressed"
 */
import { View, Text, Button, State, Root } from 'kwikui';
const state = new State({ count: 0 });

export default () => Root(
    View({
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
            color: "#fff",
            onClick: function (event) {
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
            onHoverEnter: function () {
                console.log("[Hover] Enter blue box");
            },
            onHoverLeave: function () {
                console.log("[Hover] Leave blue box");
            }
        }),
        // ── 长按区域 ──────────────────────────
        View({
            width: 350,
            height: 100,
            background: "#FF9800",
            borderRadius: 8,
            onLongPress: function (event) {
                console.log("[LongPress] orange box pressed at ("
                    + event.x.toFixed(0) + ","
                    + event.y.toFixed(0) + ")");
            }
        }),
        // ── 提示文字 ──────────────────────────
        Text({
            text: "Click green  |  Hover blue  |  Long-press orange",
            fontSize: 14,
            color: "#0000ff"
        })
    ])
);