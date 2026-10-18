// ── PageParts —— 页面自组装积木 ──
// 内容装配权在各页面（main 只供框架与净宽），这类全站一致的卡片
// 沉淀于此：标题 / 描述 / 代码卡。样式逐字对齐旧 main.pageBody，
// 页面按需拼装，顺序与尺寸由各页自定。
import { Text } from 'kwikui';
import { t } from '../lang.js';
import { codeBlock } from './code_block.js';

export const pageTitle = (meta) => Text({
    text: t(meta.title), fontSize: 22, fontWeight: 'bold',
    color: '#0F172A', margin: [0, 0, 10, 4],
});

export const pageDesc = (meta) => Text({
    text: t(meta.desc), fontSize: 13, color: '#64748B',
    margin: [0, 0, 20, 4],
});

// 代码卡 = 本页 meta.code 的集中示例摘录，高度由调用页定
export const pageCode = (meta, height = 320, w = 880) =>
    codeBlock(t, meta.code, height, w);
