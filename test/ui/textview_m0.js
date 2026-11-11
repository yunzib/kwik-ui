// TextView M0/M1 验证页：CJK 折行 + 禁则 + 斜体 + 段落属性 + 链接 + 查找高亮
// 验证点：
//   ① 长中文段（无空格）按字符断行，行宽不溢出
//   ② 行首禁则：，。、！？等不落行首（回退到上一行行尾）
//   ③ 行尾禁则：（「『 等不落行尾（下移到下一行行首）
//   ④ 中英混排：英文词组仍走空格折行
//   ⑤ 伪斜体：fontStyle: "italic" 字形右倾（M1）
//   ⑥ textAlign: center / right（M1）
//   ⑦ lineHeight 倍率行距（M1）
//   ⑧ 行内链接：readOnly 点击触发 onLinkClick（M1）
//   ⑨ 查找高亮：highlightAll / clearHighlight / findText（M1）
import { Root, View, Text, TextView, Input, State, ref, highlightAll, clearHighlight, findText } from 'kwikui';

const st = new State({ url: "（点击链接测试 onLinkClick）", find: "（输入关键词开始查找）" });

export default () => Root(View({ width: 900, height: 700, background: [250, 250, 250, 255], padding: [20, 24, 20, 24] }, [
    Text({ text: "TextView M0/M1 — 折行禁则 · 斜体 · 段落属性", fontSize: 16, color: [30, 30, 30, 255], margin: [0, 0, 6, 0] }),
    TextView({ width: 850, height: 200, content: [
        { text: "这是一段完全没有空格的超长中文文本用于验证逐字符断行是否生效，标点符号像这样，逗号在句中会出现，如果禁则生效则逗号不会出现在行首。句号也是同理。感叹号同样如此！问号呢？", fontSize: 15, textColor: "#1a1a1a" },
        { text: "\n", fontSize: 15 },
        { text: "英文混排 word wrap should still break at spaces like this example sentence does.", fontSize: 15, textColor: "#1a1a1a" },
        { text: "\n", fontSize: 15 },
        { text: "行尾禁则验证：开括号（与引号「不应落在行尾，如果它们恰好在断点处就应整体下移到下一行行首显示。", fontSize: 15, textColor: "#1a1a1a" },
        { text: "\n", fontSize: 15 },
        { text: "混合样式：", fontSize: 15, textColor: "#1a1a1a" },
        { text: "加粗", fontSize: 15, fontWeight: "bold", textColor: "#1a1a1a" },
        { text: " ", fontSize: 15 },
        { text: "下划线", fontSize: 15, underline: true, textColor: "#1a1a1a" },
        { text: " ", fontSize: 15 },
        { text: "删除线", fontSize: 15, strikethrough: true, textColor: "#1a1a1a" },
        { text: " ", fontSize: 15 },
        { text: "红字", fontSize: 15, textColor: "#e53935" },
        { text: " ", fontSize: 15 },
        { text: "斜体 Italic", fontSize: 15, fontStyle: "italic", textColor: "#1a1a1a" },
        { text: " 大字号", fontSize: 20, textColor: "#1a1a1a" },
        { text: "\n", fontSize: 15 },
        { text: "斜体加粗混排：", fontSize: 15, textColor: "#1a1a1a" },
        { text: "Emphasis words in italic style", fontSize: 15, fontStyle: "italic", textColor: "#1a1a1a" },
    ] }),
    Text({ text: "lineHeight: 2.0（默认 1.4）", fontSize: 13, color: [120, 120, 120, 255], margin: [8, 0, 4, 0] }),
    TextView({ width: 850, lineHeight: 2.0, content: [
        { text: "这段文字行距明显宽松，用于验证 lineHeight 倍率属性生效，与上方默认行距对比一目了然。", fontSize: 15, textColor: "#1a1a1a" },
    ] }),
    Text({ text: "textAlign: center", fontSize: 13, color: [120, 120, 120, 255], margin: [8, 0, 4, 0] }),
    TextView({ width: 850, textAlign: "center", content: [
        { text: "这段文字应该水平居中显示", fontSize: 15, textColor: "#1a1a1a" },
        { text: "\n", fontSize: 15 },
        { text: "短行也居中", fontSize: 15, textColor: "#1a1a1a" },
    ] }),
    Text({ text: "textAlign: right", fontSize: 13, color: [120, 120, 120, 255], margin: [8, 0, 4, 0] }),
    TextView({ width: 850, textAlign: "right", content: [
        { text: "这段文字应该靠右对齐显示", fontSize: 15, textColor: "#1a1a1a" },
    ] }),
    Text({ text: "行内链接（readOnly 点击 / 编辑态 Ctrl+点击触发 onLinkClick）", fontSize: 13, color: [120, 120, 120, 255], margin: [8, 0, 4, 0] }),
    Text({ text: ref(st, "url"), fontSize: 13, color: [25, 118, 210, 255], margin: [0, 0, 4, 0] }),
    TextView({ width: 850, readOnly: true, onLinkClick: (e) => st.update({ url: "onLinkClick: " + e.url }), content: [
        { text: "访问 ", fontSize: 15, textColor: "#1a1a1a" },
        { text: "KwiK UI 文档站", fontSize: 15, link: "https://docs.kwik-ui.example" },
        { text: " 获取更多组件说明，或查看 ", fontSize: 15, textColor: "#1a1a1a" },
        { text: "GitHub 仓库", fontSize: 15, link: "https://github.com/kwik-ui" },
        { text: "。", fontSize: 15, textColor: "#1a1a1a" },
    ] }),
    Text({ text: "查找高亮（输入关键词实时高亮；编辑文本后高亮偏移自愈）", fontSize: 13, color: [120, 120, 120, 255], margin: [8, 0, 4, 0] }),
    Input({ width: 300, height: 30, fontSize: 13, placeholder: "输入关键词（如：禁则）", margin: [0, 0, 6, 0],
        onChange: (v) => {
            if (!v) { clearHighlight("tvFind"); st.update({ find: "（输入关键词开始查找）" }); return; }
            const n = highlightAll("tvFind", v);
            const pos = findText("tvFind", v);
            st.update({ find: n > 0 ? `highlightAll("${v}") 命中 ${n} 处，findText 首个偏移 ${pos}` : `未找到 "${v}"` });
        } }),
    Text({ text: ref(st, "find"), fontSize: 13, color: [120, 120, 120, 255], margin: [0, 0, 4, 0] }),
    TextView({ id: "tvFind", width: 850, content: [
        { text: "禁则验证：行首禁则禁止标点落行首，行尾禁则禁止开括号落行尾，禁则处理让排版更规范。输入其他关键词试试，直接编辑本文后高亮会自动跟随偏移重算。", fontSize: 15, textColor: "#1a1a1a" },
    ] }),
]));
