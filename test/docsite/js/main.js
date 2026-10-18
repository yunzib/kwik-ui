// ── 文档站主应用 ──
// 双栏布局：顶栏（品牌 + 语言切换）+ 左导航（分组条目，Button 扁平面）+
// 右内容区（页面自治：各页 content() 自行组装全部卡片，ScrollView）。
// 页面注册表 PAGES 控制可见面集；组标签按注册顺序渲染，空组自动隐藏。
// 语言切换写 langState、切页写 nav——均为 State 写 → 框架整树 reconcile，
// 读过这些态的节点随扩展翻转。
import {
    Root, View, Text, Button, Flex, ScrollView, State, getWindowSize,
} from 'kwikui';
import { langState, switchLang, t } from './lang.js';
import * as indexPage from './pages/index.js';
import * as viewPage from './pages/view.js';
import * as buttonPage from './pages/button.js';

const PAGES = [indexPage, viewPage, buttonPage];
const PAGE_BY_ID = Object.fromEntries(PAGES.map(p => [p.meta.id, p]));

const GROUP_ORDER = ['docs', 'base', 'form', 'layout', 'data', 'adv'];
const GROUPS = {
    docs:   { zh: '文档',   en: 'Docs' },
    base:   { zh: '基础',   en: 'Basic' },
    form:   { zh: '表单',   en: 'Form' },
    layout: { zh: '布局',   en: 'Layout' },
    data:   { zh: '数据',   en: 'Data' },
    adv:    { zh: '高级',   en: 'Advanced' },
};

const nav = new State({ page: 'index' });

// 窗口尺寸态：根声明 onWindowResize（框架仅根组件生效）→ 事件写 State →
// 整树 reconcile → 读它的节点随窗口缩放重排（响应式布局的框架正道）
const winSize = new State({ w: 0, h: 0 });

// ── 左导航条目（选中态读 nav.page → 随选随翻）──
const navItem = (m) => {
    const on = nav.page === m.meta.id;    // m 为页模块（namespace），经 m.meta 取页元数据
    return Button({
        text: t(m.meta.title),
        width: 168, height: 30, fontSize: 13,
        background: on ? '#2563EB' : '#F1F5F9',
        color: on ? '#FFFFFF' : '#475569',
        borderRadius: 6, margin: [0, 0, 3, 0],
        onClick: () => { nav.page = m.meta.id; },
    });
};

const navPane = () => {
    const groups = GROUP_ORDER.filter(g => PAGES.some(p => (p.meta.group ?? 'docs') === g));
    const children = groups.flatMap(g => [
        Text({ text: t(GROUPS[g]), fontSize: 11, color: '#94A3B8', margin: [14, 0, 6, 12] }),
        ...PAGES.filter(p => (p.meta.group ?? 'docs') === g).map(navItem),
    ]);
    return ScrollView({
        direction: 'vertical', width: 200,
        background: '#FFFFFF', padding: [12, 0, 16, 16], showScrollbar: true,
    }, [Flex({ direction: 'column' }, children)]);    // 双契约：① children 必须数组（裸对象被 parseNode 静默跳过变叶子）；② ScrollView 不流式排版（子节点按 x/y 定位），多内容包一层 Flex 作唯一内容子节点
};

// ── 内容区：页面自治组装——content() 由各页定义并负责全部卡片
// （标题/描述/实况卡/代码卡/属性表的顺序与尺寸都是页面自己的事）；
// main 只负责框架壳与净宽下传，缺页/缺 content() 时兜底提示 ──
// innerW = 内容区净宽（contentInner），下传各卡片工厂随窗口缩放
const pageBody = (p, innerW) =>
    p && p.content ? p.content(innerW)
        : [Text({
            text: p ? 'page missing content()' : 'page not found',
            fontSize: 14, color: '#EF4444',
        })];

const contentPane = () => {
    return ScrollView({
        direction: 'vertical', width: winSize.w - 200,    // 读 winSize（与根布局同源），不直查窗口
        background: '#F5F7FB', padding: [18, 12, 32, 28], showScrollbar: true,
    }, [Flex({ direction: 'column' }, pageBody(PAGE_BY_ID[nav.page], contentInner()))]);    // 同 navPane：children 数组 + Flex 内容节点双契约；净宽下传 pageBody 全量卡片
};

// getWindowSize 依赖运行期窗钩（headless/极早期为 undefined），回退默认
const getWinSize = () => {
    const w = getWindowSize();
    return w ? { w: w.width, h: w.height } : { w: 1280, h: 820 };
};

// ── 内容区净宽（单一尺寸源）：窗口宽 − 导航 200 − 内容区 padding 左 18/右 32；
// 最小宽 420 兜底，窗口过窄时由纵向滚动吸收（防卡片碎片化）──
const contentInner = () => Math.max(winSize.w - 200 - 18 - 32, 420);

// ── 顶栏：品牌 + 语言切换（按钮文案读 langState → 双向见当前语言）──
const header = (W) => {
    return Flex({
        direction: 'row', justifyContent: 'spaceBetween', alignItems: 'center',
        width: W, height: 52, background: '#FFFFFF', padding: [0, 20, 0, 16],
    }, [
        Text({ text: 'KwiK UI Docs', fontSize: 17, fontWeight: 'bold', color: '#0F172A' }),
        Button({
            text: langState.lang === 'zh' ? 'EN' : '中文',
            width: 64, height: 30, fontSize: 13,
            background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
            onClick: () => switchLang(),
        }),
    ]);
};

export default () => {
    if (winSize.w === 0) {    // 首帧：经窗口查询初始化
        const s0 = getWinSize();
        winSize.w = s0.w;
        winSize.h = s0.h;
    }
    const W = winSize.w, H = winSize.h;
    return Root({
        // 窗口缩放 → {width,height} 写 State → 树重扩展 → 宽高消费点联动
        onWindowResize: (e) => { 
            console.log('onWindowResize', e.width, e.height);
            winSize.w = e.width; winSize.h = e.height; },
            
    }, View({ width: W, height: H, background: '#F5F7FB' }, [
        header(W),
        Flex({ direction: 'row', width: W, height: H - 52 }, [
            navPane(),
            contentPane(),
        ]),
    ]));
};
