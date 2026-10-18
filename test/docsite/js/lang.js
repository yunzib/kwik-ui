// ── 文档站语言帮手 ──
// 语言态全局单源：langState.lang 变更 → 读它的节点整树重扩展（框架
// State 响应链），全站文案随之翻转。词表按页内聚（pages/*),本文件只
// 提供切面。
import { State } from 'kwikui';

export const langState = new State({ lang: 'zh' });

/** 取双语词表的当前语言文案（缺语言回退 zh） */
export const t = (obj) => {
    if (typeof obj === 'string') return obj;
    return obj[langState.lang] ?? obj.zh ?? '';
};

/** zh ⇄ en 行内切换 */
export const switchLang = () => {
    langState.lang = langState.lang === 'zh' ? 'en' : 'zh';
};
