// TextView 滚动验证页：ScrollView 页面级滚动 + 查找自动翻滚（revealText）
// 验证点：
//   ① 页面级滚动：滚轮 / 拖右侧滚动条 / 按住内容拖动（带 fling 惯性）均可上下
//   ② 整篇查找：textViewIds 枚举文档内全部 TextView → 逐组件 highlightAll/
//      findText → 汇总匹配列表（文档序）→ 第 k/N 处导航 + revealText 自动翻滚
//   ③ 上一个/下一个导航：匹配列表索引回绕闭环，每步自动翻滚到当前命中
//   ④ 查找栏固定不滚：输入框/导航按钮/状态行在 ScrollView 之外
//   ⑤ 长文档排版：CJK 折行/禁则、中英混排、lineHeight、居中/右对齐、行内链接
import { Root, View, Flex, Text, TextView, Input, Button, ScrollView, State, ref,
         highlightAll, clearHighlight, findText, revealText, textViewIds,
         setActiveMatch } from 'kwikui';

const st = new State({ find: "（输入关键词 → 整篇高亮命中并可 上一处/下一处 导航）", url: "（点击链接测试 onLinkClick）", });

// 整篇查找游标（模块级：State 重建/reconcile 后延续；每步 st.update 刷新状态行）
let needle = "", total = 0, curIdx = -1;
let matches = [];    // [{ id, pos }] 文档序（跨组件聚合；pos = 组件内字节偏移）
let activeId = "";   // 当前命中所在组件（导航换段时先清除前一项的深色命中）

const utf8Len = (s) => {    // JS 字符串 → UTF-8 字节长（偏移推进与组件内口径一致）
    let n = 0;
    for (const ch of s) {
        const c = ch.codePointAt(0);
        n += c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
    }
    return n;
};

const docIds = () => textViewIds("doc");    // 文档序枚举可查组件（doc 子树）

// 当前命中 = 深琥珀（setActiveMatch），其余命中 = 浅琥珀（highlightAll）。
// 跨组件导航时先清除前一项所在组件的 active 区间，再设置新项。
function markActive() {
    if (activeId) setActiveMatch(activeId, 0, 0);
    activeId = total > 0 ? matches[curIdx].id : "";
    if (activeId) setActiveMatch(activeId, matches[curIdx].pos, Math.max(1, utf8Len(needle)));
}

const navLabel = () =>
    total > 0 ? `命中 ${total} 处 · 第 ${curIdx + 1}/${total} 处（${matches[curIdx].id} 偏移 ${matches[curIdx].pos}），已自动滚到当前命中`
              : "（输入关键词 → 整篇高亮命中并可 上一处/下一处 导航）";

function doFind(v) {
    needle = v;
    matches = [];
    total = 0;
    curIdx = -1;
    if (!v) {
        for (const id of docIds()) clearHighlight(id);
        if (activeId) setActiveMatch(activeId, 0, 0);
        activeId = "";
        st.update({ find: navLabel() });
        return;
    }
    const len = Math.max(1, utf8Len(v));
    for (const id of docIds()) {
        const n = highlightAll(id, v);    // 高亮本组件全部命中
        if (!n) continue;
        let from = 0;
        for (let k = 0; k < n; k++) {     // 逐命中取偏移（非重叠推进＝highlightAll 口径）
            const pos = findText(id, v, from);
            if (pos < 0) break;
            matches.push({ id, pos });
            from = pos + len;
        }
    }
    total = matches.length;
    if (total > 0) {
        curIdx = 0;
        revealText(matches[0].id, matches[0].pos);    // 自动翻滚到首命中
        markActive();                                  // 当前项深色高亮
        st.update({ find: navLabel() });
    } else {
        if (activeId) setActiveMatch(activeId, 0, 0);
        activeId = "";
        st.update({ find: `未找到 "${v}"` });
    }
}

function step(dir) {
    if (total <= 0) return;
    curIdx = (curIdx + dir + total) % total;    // 首尾回绕闭环
    revealText(matches[curIdx].id, matches[curIdx].pos);
    markActive();
    st.update({ find: navLabel() });
}

export default () => Root(View({ width: 900, height: 700, background: [250, 250, 250, 255], padding: [20, 24, 20, 24] }, [
    Text({ text: "TextView 滚动 + 查找定位（ScrollView · revealText · 上一处/下一处）", fontSize: 16, color: [30, 30, 30, 255], margin: [0, 0, 4, 0] }),
    Text({ text: "滚轮 / 右侧滚动条 / 按住内容拖动；输入关键词后整篇高亮（当前命中深琥珀、其余浅琥珀），可用 上一处/下一处 逐条跳转并自动翻滚",
           fontSize: 12, color: [120, 120, 120, 255], margin: [0, 0, 6, 0] }),
    Flex({ direction: "row", gap: 8, margin: [0, 0, 6, 0] }, [
        Input({ width: 400, height: 30, fontSize: 13, placeholder: "输入关键词（如：禁则 / 高亮）",
            onChange: (v) => doFind(v) }),
        Button({ text: "上一处", width: 80, height: 30, color: "#ffffff", onClick: () => step(-1) }),
        Button({ text: "下一处", width: 80, height: 30, color: "#ffffff", onClick: () => step(1) }),
    ]),
    Text({ text: ref(st, "find"), fontSize: 13, color: [25, 118, 210, 255], margin: [0, 0, 6, 0] }),

    // ═══ 可滚动文档区（视口 566 < 内容总高 → 纵向可滚）═══════════════════
    ScrollView({ direction: "vertical", height: 566, background: [255, 255, 255, 255], borderRadius: 8 }, [
        View({ id: "doc", width: 850 }, [
            Text({ text: "① CJK 逐字折行 + 行首/行尾禁则", fontSize: 13, color: [120, 120, 120, 255], margin: [8, 0, 4, 0] }),
            TextView({ id: "p1", width: 850, content: [
                { text: "这是一段完全没有空格的超长中文文本用于验证逐字符断行是否生效，标点符号像这样，逗号在句中会出现，如果禁则生效则逗号不会出现在行首。句号也是同理。感叹号同样如此！问号呢？开括号（与引号「不应落在行尾，如果它们恰好在断点处就应整体下移到下一行行首显示。连续标点也应保持禁则：」，。！？这些都不会出现在行首。", fontSize: 15, textColor: "#1a1a1a" },
            ] }),

            Text({ text: "② 中英混排 word wrap", fontSize: 13, color: [120, 120, 120, 255], margin: [8, 0, 4, 0] }),
            TextView({ id: "p2", width: 850, content: [
                { text: "英文混排 word wrap should still break at spaces like this example sentence does, and CJK 中文与 English 混排时按字符断行而英文单词保持完整不被拆开，long-word-without-spaces-such-as-this-one 超长单词整体换行不溢出。", fontSize: 15, textColor: "#1a1a1a" },
            ] }),

            Text({ text: "③ 混合样式 + 伪斜体（M1）", fontSize: 13, color: [120, 120, 120, 255], margin: [8, 0, 4, 0] }),
            TextView({ id: "p3", width: 850, content: [
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
            ] }),

            Text({ text: "④ lineHeight 2.0（默认 1.4）", fontSize: 13, color: [120, 120, 120, 255], margin: [8, 0, 4, 0] }),
            TextView({ id: "p4", width: 850, lineHeight: 2.0, content: [
                { text: "这段文字行距明显宽松，用于验证 lineHeight 倍率属性生效，与相邻段落的默认行距对比一目了然。", fontSize: 15, textColor: "#1a1a1a" },
            ] }),

            Text({ text: "⑤ textAlign: center", fontSize: 13, color: [120, 120, 120, 255], margin: [8, 0, 4, 0] }),
            TextView({ id: "p5", width: 850, textAlign: "center", content: [
                { text: "这段文字应该水平居中显示", fontSize: 15, textColor: "#1a1a1a" },
                { text: "\n", fontSize: 15 },
                { text: "短行也居中", fontSize: 15, textColor: "#1a1a1a" },
            ] }),

            Text({ text: "⑥ textAlign: right", fontSize: 13, color: [120, 120, 120, 255], margin: [8, 0, 4, 0] }),
            TextView({ id: "p6", width: 850, textAlign: "right", content: [
                { text: "这段文字应该靠右对齐显示", fontSize: 15, textColor: "#1a1a1a" },
            ] }),

            Text({ text: "⑦ 行内链接（readOnly 点击触发 onLinkClick）", fontSize: 13, color: [120, 120, 120, 255], margin: [8, 0, 4, 0] }),
            Text({ text: ref(st, "url"), fontSize: 13, color: [25, 118, 210, 255], margin: [0, 0, 4, 0] }),
            TextView({ id: "p7", width: 850, readOnly: true, onLinkClick: (e) => st.update({ url: "onLinkClick: " + e.url }), content: [
                { text: "访问 ", fontSize: 15, textColor: "#1a1a1a" },
                { text: "KwiK UI 文档站", fontSize: 15, link: "https://docs.kwik-ui.example" },
                { text: " 获取更多组件说明，或查看 ", fontSize: 15, textColor: "#1a1a1a" },
                { text: "GitHub 仓库", fontSize: 15, link: "https://github.com/kwik-ui" },
                { text: "。", fontSize: 15, textColor: "#1a1a1a" },
            ] }),

            Text({ text: "⑧ 长文档续页（滚动溢出的长度保障）", fontSize: 13, color: [120, 120, 120, 255], margin: [8, 0, 4, 0] }),
            TextView({ id: "p8a", width: 850, content: [
                { text: "长文档阅读是滚动视口的核心场景：当文章总高超过可视区域时，滚轮、滚动条与内容拖拽三条通路都应顺滑可用，且滚动到底后继续滚轮不得越界（clamp 收口）。本段用于把文档总高推过视口高度，确保进入本页即可看到右侧滚动条出现。段落本身没有特殊排版要求，只需验证长文在固定宽度内稳定折行、行距均匀、滚动时无残影无跳变。", fontSize: 15, textColor: "#1a1a1a" },
            ] }),
            TextView({ id: "p8b", width: 850, content: [
                { text: "继续追加一段中英混排的长文本以补足高度：scroll viewport demonstration content, 用于确认滚动条比例滑块随内容高度正确缩放（轨道长度固定、滑块最短 24px），拖动滑块时内容按比例跟随、松手不跳位；同时确认滚轮每格 30px 步进在长文档中手感连续。查找联动依赖的 revealText 只修改滚动偏移、不改变文档排版，因此往返揭示不会造成文字重排或高亮丢失。", fontSize: 15, textColor: "#1a1a1a" },
            ] }),

            Text({ text: "⑨ 查找目标段（整篇查找覆盖本节）", fontSize: 13, color: [120, 120, 120, 255], margin: [8, 0, 4, 0] }),
            TextView({ id: "tvFind", width: 850, content: [
                { text: "禁则验证：行首禁则禁止标点落行首，行尾禁则禁止开括号落行尾，禁则处理让排版更规范。输入其他关键词试试，直接编辑本文后高亮会自动跟随偏移重算。查找覆盖整篇文档（textViewIds 枚举 + 逐组件聚合）：输入“禁则”可看到命中分散在多个段落（第 k/N 处跨组件计数），每步 上一处/下一处 都会把当前命中滚进视口。本段只是文档末尾的命中示例之一。", fontSize: 15, textColor: "#1a1a1a" },
            ] }),
        ]),
    ]),
]));
