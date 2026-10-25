// ── PageKit —— 页面装配积木（只供积木，不代装配）──
// 每个页面文件自持 meta + content()，像 view.js / propbus.js 那样自己决定
// 卡片顺序；这里只沉淀全站一致的“卡”与“小节”，避免样式各写一份。
import { View, Flex, Text } from 'kwikui';
import { t } from '../lang.js';
import { codeBlock } from './code_block.js';

// 代码高度按行数自适应（行高 19 + 上下 padding 24 + 余量）
export const codeH = (code) => code.split('\n').length * 19 + 26;

// 代码卡：与本页示例代码同源，宽度跟随内容区
export const codeCard = (code, innerW) =>
    codeBlock(t, code, codeH(code), innerW);

// ── 双列代码对照（JS / C++ 同一功能点并排）──
// 供 channel 这类"两端都要写一遍"的页面使用：左列 JS、右列 C++，各带语言
// 标签条。单列宽 = (innerW − 12) / 2；内容区 < 720 时退化为上下堆叠，避免
// 列窄到代码必须硬折行。两列行数取最大值，短的一侧补空行保持等高。
const ACCENT = { js: '#FACC15', cpp: '#7DD3FC', bad: '#F87171', good: '#4ADE80' };

const pairCol = (col, w, lines) => View({
    width: w, background: '#0B1220',
    borderColor: '#1E2A44', borderWidth: 1, borderRadius: 8,
}, [
    // 标签条：语言色点 + 端名 + 该端 API（sub）
    Flex({
        direction: 'row', alignItems: 'center', gap: 6,
        width: w - 2, height: 30, background: '#101A30',
        padding: [0, 10, 0, 10],
    }, [
        Text({ text: '●', fontSize: 9, color: ACCENT[col.accent] || '#94A3B8' }),
        Text({ text: col.tag, fontSize: 11.5, fontWeight: 'bold', color: '#E2E8F0' }),
        // 副标题（该端 API 名）可缺省——标签本身够长时不再占位，避免挤出标签条
        ...(col.sub ? [Text({ text: col.sub, fontSize: 10.5, color: '#64748B', width: w - 130 })] : []),
    ]),
    // 正文：逐行一个 Text（行高 18 恒定，格式即源文本；不换行以免破坏缩进）
    Flex({ direction: 'column', width: w - 2, padding: [10, 10, 10, 10] },
        lines.map((ln) => Text({
            text: ln.length ? ln : ' ',
            width: w - 22, height: 18, fontSize: 11.5, color: '#C9D6F0',
            selectable: true,
        }))),
]);

/**
 * JS / C++ 并排对照卡
 * @param left   { tag, sub, accent, code }
 * @param right  同上
 * @param innerW 内容区净宽
 */
export const codePair = (left, right, innerW) => {
    const toLines = (c) => c.replace(/\r/g, '').split('\n');
    const side = innerW >= 720;
    const w = side ? Math.floor((innerW - 12) / 2) : innerW;
    const cols = [left, right].map((c) => toLines(c.code));
    const n = Math.max(cols[0].length, cols[1].length);
    const pad = (a) => a.concat(new Array(n - a.length).fill(''));
    return Flex({
        direction: side ? 'row' : 'column',
        gap: side ? 12 : 10,
        width: innerW, margin: [0, 0, 24, 4],
    }, [pairCol(left, w, pad(cols[0])), pairCol(right, w, pad(cols[1]))]);
};

// 演示块内小标签
export const label = (txt, dark) => Text({
    text: txt, fontSize: 11.5, color: dark ? '#334155' : '#FFFFFF',
    padding: [4, 0, 0, 8],
});

// 标准演示卡：白底、细边框、圆角，宽度跟随内容区
export const demoCard = (children, innerW) => View({
    width: innerW, padding: 16, background: '#FFFFFF',
    borderColor: '#E2E8F0', borderWidth: 1, borderRadius: 8,
    margin: [0, 0, 8, 4],
}, Array.isArray(children) ? children : [children]);

// 说明段落（不套卡片，保持阅读节奏）
export const note = (text, innerW, color = '#64748B') => Text({
    text, fontSize: 12, color, width: innerW - 32, wordWrap: true,
    margin: [2, 0, 10, 2],
});

// 示例小节：小节标题 → 演示卡 → 紧随其下的示例代码卡
export const section = (title, children, code, innerW) => [
    Text({ text: title, fontSize: 15, fontWeight: 'bold', color: '#0F172A', margin: [14, 0, 8, 4] }),
    demoCard(children, innerW),
    codeCard(code, innerW),
];

// 语义标签（用于主题 token / 状态标记）
export const chip = (text, bg, fg) => Flex({
    direction: 'row', alignItems: 'center', justifyContent: 'center',
    height: 26, padding: [0, 10, 0, 10], borderRadius: 13,
    background: bg, margin: [0, 0, 8, 0],
}, [Text({ text, fontSize: 11.5, color: fg })]);
