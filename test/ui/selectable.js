// ── Text 文字选区 ──
// 交互清单：
//   ① 拖选：段落文字按下拖动出高亮，Ctrl+C 复制到系统剪贴板
//   ② 双击选词：ASCII 词/标点段、中文连续段整体
//   ③ Ctrl+A：全选当前宿主段落（再 Ctrl+C 带走）
//   ④ 互斥：左/右两段独立可选，开新选区旧高亮即擦
//   ⑤ ScrollView 内拖选：拖动被选区消费（容器不滚），滚轮照常滚动
//   ⑥ 跨重建保留：点按计数触发整树 reconcile，选区随复用节点存活
import { Root, View, Flex, Text, Button, ScrollView, State } from 'kwikui';

// 模块顶层 State（整树重建间存活，计数驱动 reconcile 验证选区保留）
const st = new State({ n: 0 });

const PARA_ZH = '声明式组件库的展示文本现在可以选中了：按住左键拖动出高亮，双击选中一个词，Ctrl+C 复制到系统剪贴板，Ctrl+A 全选本段。点右侧计数按钮触发整树重建，选区不会丢。';
const PARA_EN = 'Display text is now selectable: drag to highlight, double-click to pick a word, Ctrl+C to copy, Ctrl+A to select the whole paragraph. Clicking the counter rebuilds the tree — the selection survives.';

// 可选段落卡：标题（不可选）+ 正文（selectable）
const card = (title, body, extraProps = {}) => View({
    width: 560, padding: 16, background: '#FFFFFF',
    borderColor: '#E2E8F0', borderWidth: 1, borderRadius: 8,
    ...extraProps,
}, [
    Text({ text: title, fontSize: 13, color: '#94A3B8', margin: [0, 0, 8, 0] }),
    Text({ text: body, fontSize: 15, selectable: true, wordWrap: true, lineHeight: 26 }),
]);

export default () => Root(View({ width: 960, height: 900, background: '#F5F7FB', padding: 24 }, [
    Text({ text: 'Text 文字选区', fontSize: 20, fontWeight: 'bold', margin: [0, 0, 6, 0] }),
    Text({
        text: '拖选出高亮 → Ctrl+C 复制；双击选词；Ctrl+A 全选本段；滚动容器内拖选不滚、滚轮照常。',
        fontSize: 12, color: '#64748B', margin: [0, 0, 16, 0],
    }),

    Flex({ direction: 'row', gap: 16 }, [
        card('①② 段落拖选（中文）', PARA_ZH),
        View({ width: 560 }, [
            card('①② 段落拖选（英文）', PARA_EN, { margin: [0, 0, 16, 0] }),
            // ⑥ 计数联动：State 写 → 整树 reconcile → 选区随复用节点保留
            Flex({ direction: 'row', alignItems: 'center' }, [
                Button({
                    text: '重建计数: ' + st.n, width: 140, height: 32, fontSize: 13,
                    background: '#2563EB', color: '#FFFFFF', borderRadius: 6,
                    onClick: () => { st.n = st.n + 1; },
                }),
                Text({
                    text: '选中文字后点我：焦点转移即清选区（选区随焦点）',
                    fontSize: 12, color: '#909399', padding: [0, 0, 0, 12],
                }),
            ]),
        ]),
    ]),

    // ⑤ 滚动容器内选区：拖动消费给选区（容器不滚），滚轮正常滚
    Text({ text: '④⑤ ScrollView 内拖选（滚轮滚动 / 拖动选择）', fontSize: 13, color: '#94A3B8', margin: [16, 0, 8, 0] }),
    ScrollView({
        direction: 'vertical', height: 220, background: '#FFFFFF',
        borderColor: '#E2E8F0', borderWidth: 1, borderRadius: 8, showScrollbar: true,
    }, [Flex({ direction: 'column', padding: 16 }, [
        Text({
            text: PARA_ZH + '\n' + PARA_EN + '\n' + PARA_ZH + '\n' + PARA_EN,
            fontSize: 14, selectable: true, wordWrap: true, lineHeight: 24,
        }),
    ])]),
]));
