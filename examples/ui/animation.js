import { View, Text, Button, Flex, animate, stop, isAnimating } from 'kwikui';

const $ = globalThis.__animState ??= {};

export default View({
    width: 800,
    height: 600,
    background: "#f0f2f5",
    padding: 14
}, [
    Text({
        text: '命令式动画演示', fontSize: 22, fontWeight: 'bold',
        color: '#303133', margin: [0, 0, 4, 0]
    }),
    Text({
        text: '通过 animate() 驱动属性变化，支持缓动、关键帧、循环',
        fontSize: 13, color: '#C0C4CC', margin: [0, 0, 14, 0]
    }),

    Flex({ direction: "row", gap: 10, margin: [0, 0, 10, 0] }, [
        // ── 弹簧 ──
        View({ width: 250, height: 280, background: "#fff", borderRadius: 8,
               padding: 14, overflow: "hidden" }, [
            View({ id: 'spring-box', width: 120, height: 100, background: '#409EFF',
                   borderRadius: 8, align: "center" }, [
                Text({ text: '弹簧', fontSize: 16, fontWeight: 'bold', color: '#FFFFFF' })
            ]),
            Button({
                text: 'Spring 弹簧', background: '#337ECC', color: '#FFFFFF',
                width: 222, height: 36, borderRadius: 4, fontSize: 14,
                margin: [8, 0, 0, 0],
                onClick: () => {
                    $.spring = !$.spring;
                    animate('spring-box',
                        { scale: $.spring ? 1.8 : 1.0 },
                        { duration: 0.7, easing: 'spring(160,12)' });
                }
            }),
        ]),
        // ── 淡出 ──
        View({ width: 250, height: 280, background: "#fff", borderRadius: 8,
               padding: 14, overflow: "hidden" }, [
            View({ id: 'fade-box', width: 222, height: 170, background: '#E6A23C',
                   borderRadius: 8, align: "center" }, [
                Text({ text: '淡出', fontSize: 16, fontWeight: 'bold', color: '#FFFFFF' })
            ]),
            Button({
                text: '淡出', background: '#C78A2E', color: '#FFFFFF',
                width: 222, height: 36, borderRadius: 4, fontSize: 14,
                margin: [8, 0, 0, 0],
                onClick: () => {
                    $.fade = !$.fade;
                    animate('fade-box',
                        { opacity: $.fade ? 0.2 : 1.0 },
                        { duration: 0.6, easing: 'easeOut' });
                }
            }),
        ]),
        // ── 圆角 ──
        View({ width: 250, height: 280, background: "#fff", borderRadius: 8,
               padding: 14, overflow: "hidden" }, [
            View({ id: 'radius-box', width: 222, height: 170, background: '#F56C6C',
                   borderRadius: 8, align: "center" }, [
                Text({ text: '圆角', fontSize: 16, fontWeight: 'bold', color: '#FFFFFF' })
            ]),
            Button({
                text: '圆角', background: '#D46060', color: '#FFFFFF',
                width: 222, height: 36, borderRadius: 4, fontSize: 14,
                margin: [8, 0, 0, 0],
                onClick: () => {
                    $.radius = !$.radius;
                    animate('radius-box',
                        { borderRadius: $.radius ? 75 : 8 },
                        { duration: 0.5, easing: 'easeInOut' });
                }
            }),
        ]),
    ]),

    Flex({ direction: "row", gap: 10 }, [
        // ── 组合动画 ──
        View({ width: 250, height: 280, background: "#fff", borderRadius: 8,
               padding: 14, overflow: "hidden" }, [
            View({ id: 'combo-box', width: 120, height: 100, background: '#409EFF',
                   borderRadius: 8, align: "center" }, [
                Text({ text: '组合动画', fontSize: 16, fontWeight: 'bold', color: '#FFFFFF' })
            ]),
            Flex({ direction: "row", gap: 8, margin: [8, 0, 0, 0] }, [
                Button({
                    text: '组合', background: '#67C23A', color: '#FFFFFF',
                    height: 36, borderRadius: 4, fontSize: 14, flex: 1,
                    onClick: () => {
                        $.combo = !$.combo;
                        animate('combo-box',
                            $.combo
                                ? { scale: 1.6, opacity: 0.4, borderRadius: 75, background: '#67C23A' }
                                : { scale: 1.0, opacity: 1.0, borderRadius: 8, background: '#409EFF' },
                            { duration: 0.6, easing: 'spring(200,15)' });
                    }
                }),
                Button({
                    text: '重置', background: '#909399', color: '#FFFFFF',
                    height: 36, borderRadius: 4, fontSize: 14, flex: 1,
                    onClick: () => {
                        $.combo = false;
                        animate('combo-box',
                            { scale: 1, opacity: 1, borderRadius: 8, background: '#409EFF' },
                            { duration: 0.4, easing: 'ease' });
                    }
                }),
            ]),
        ]),
        // ── 关键帧 ──
        View({ width: 250, height: 280, background: "#fff", borderRadius: 8,
               padding: 14, overflow: "hidden" }, [
            View({ id: 'blink-box', width: 222, height: 170, background: '#FF9800',
                   borderRadius: 8, align: "center" }, [
                Text({ text: '关键帧', fontSize: 16, fontWeight: 'bold', color: '#FFFFFF' })
            ]),
            Flex({ direction: "row", gap: 8, margin: [8, 0, 0, 0] }, [
                Button({
                    text: '闪烁', background: '#FF9800', color: '#FFFFFF',
                    height: 36, borderRadius: 4, fontSize: 14, flex: 1,
                    onClick: () => animate('blink-box',
                        { opacity: [1, 0.2, 1, 0.2, 1] },
                        { duration: 1.0, keyframes: [0, 0.25, 0.5, 0.75, 1], easing: 'easeInOut' })
                }),
                Button({
                    text: '停止', background: '#F44336', color: '#FFFFFF',
                    height: 36, borderRadius: 4, fontSize: 14, flex: 1,
                    onClick: () => stop('blink-box')
                }),
            ]),
        ]),
        // ── 往返 ──
        View({ width: 250, height: 280, background: "#fff", borderRadius: 8,
               padding: 14, overflow: "hidden" }, [
            View({ id: 'bounce-box', width: 120, height: 100, background: '#9C27B0',
                   borderRadius: 8, align: "center" }, [
                Text({ text: '往返缩放', fontSize: 16, fontWeight: 'bold', color: '#FFFFFF' })
            ]),
            Flex({ direction: "row", gap: 8, margin: [8, 0, 0, 0] }, [
                Button({
                    text: '往返', background: '#9C27B0', color: '#FFFFFF',
                    height: 36, borderRadius: 4, fontSize: 14, flex: 1,
                    onClick: () => {
                        $.bounce = !$.bounce;
                        animate('bounce-box',
                            { scale: $.bounce ? 1.6 : 1.0 },
                            { duration: 0.5, easing: 'easeOut' });
                    }
                }),
                Button({
                    text: '查询', background: '#607D8B', color: '#FFFFFF',
                    height: 36, borderRadius: 4, fontSize: 14, flex: 1,
                    onClick: () => {
                        console.log('bounce-box:', isAnimating('bounce-box'));
                        console.log('  scale:', isAnimating('bounce-box', 'scale'));
                    }
                }),
            ]),
        ]),
    ]),
]);