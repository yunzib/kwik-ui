// ── CodeBlock —— 示例代码卡 ──
// 展示入参集中的示例代码文本。标题条右侧两钮：复制（写系统剪贴板，
// 走 kwikui.setClipboardText；日志/失败面自兜底）与 展开/收起（收起只留
// 标题条）。卡片态由模块级缓存持有：同一份代码跨树重建复用同一 State
// 实例（factory 内 new State 每轮重置的陷阱，语义同各页顶层 State 约定）。
// 正文**按 \n 逐行拆分、每行一个 Text**：行数与缩进严格等于源文本，恒定
// 行高 19 自成行距，不依赖断行引擎的软+硬标记叠加次序（shaper 的 LF 标记
// 已修为按源码点判定——曾因 NotoSansSC 把 LF 映射成 gid 1 而整链失效；
// 文档站仍保留逐行拆分，属确定性取胜的工程选择）。注：仓库当前无等宽
// 字体资产（resources/fonts 仅 NotoSansSC），代码先以比例字直排——等宽
// 资产到位后此处仅换字体零改动收编。
import { View, Flex, Text, Button, ScrollView, State, setClipboardText } from 'kwikui';

// 卡片状态缓存：codeText → State({ open, copied })
const cardStates = new Map();
const cardState = (codeText) => {
    if (!cardStates.has(codeText))
        cardStates.set(codeText, new State({ open: true, copied: false }));
    return cardStates.get(codeText);
};

export const codeBlock = (t, codeText, height = 320, w = 880) => {
    // w = 卡片外宽（内容区净宽，调用方下传；默认值保持本模块可独立调用）
    const st = cardState(codeText);
    const innerW = w - 2 - 24;    // 卡边框 2 + 滚区 padding 左右各 12 → 正文可用宽

    // 标题条：卡名居左，复制/收起居右
    const titleBar = Flex({
        direction: 'row', justifyContent: 'spaceBetween', alignItems: 'center',
        width: w - 2, height: 36, background: '#101A30', padding: [0, 8, 0, 12],
    }, [
        Text({
            text: t({ zh: '示例代码', en: 'Example code' }),
            fontSize: 12, color: '#7C8DB5',
        }),
        Flex({ direction: 'row' }, [
            Button({
                text: st.copied ? t({ zh: '已复制 ✓', en: 'Copied ✓' }) : t({ zh: '复制', en: 'Copy' }),
                width: 58, height: 24, fontSize: 12,
                background: st.copied ? '#1D3A6E' : '#0B1220',
                color: st.copied ? '#8FD3A8' : '#7C8DB5', borderRadius: 5,
                onClick: () => { st.copied = !!setClipboardText(codeText); },
            }),
            Button({
                text: st.open ? t({ zh: '收起', en: 'Collapse' }) : t({ zh: '展开', en: 'Expand' }),
                width: 48, height: 24, fontSize: 12,
                background: '#0B1220', color: '#7C8DB5', borderRadius: 5,
                margin: [0, 0, 0, 8],
                onClick: () => { st.open = !st.open; st.copied = false; },
            }),
        ]),
    ]);

    // 正文：源文本逐行一个 Text（strip \r 防 CRLF 混入；空行以空格占位
    // 保行高），行高固定 19 = 恒定行距；显式行高+行宽双定，格式即源文本
    const lines = codeText.replace(/\r/g, "").split("\n");
    const body = st.open ? [ScrollView({
        direction: 'vertical', width: w - 2, height,
        background: '#0B1220', showScrollbar: true,
    }, [
        // ScrollView 子节点按 x/y 定位（非流式）→ 唯一内容子节点包一层 Flex（框架契约，同 main 双契约）
        Flex({ direction: 'column', width: w - 2, padding: [12, 12, 12, 12] },
            lines.map((ln) => Text({
                text: ln.length ? ln : ' ',
                width: innerW, height: 19, fontSize: 12.5, color: '#C9D6F0',
                selectable: true,    // 行内拖选/双击选词/Ctrl+C（与标题条复制钮互补）
            }))),
    ])] : [];

    return View({
        width: w, margin: [0, 0, 24, 4],
        background: '#0B1220', borderColor: '#1E2A44',
        borderWidth: 1, borderRadius: 8,
    }, [titleBar, ...body]);
};
