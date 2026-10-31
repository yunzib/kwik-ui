// ── 文档站主应用 ──
// 双栏布局：顶栏（品牌 + 语言切换）+ 左导航（两级折叠多级菜单，Expansion
// 嵌套）+ 右内容区（页面自治：各页 content() 自行组装全部卡片，ScrollView）。
//
// 左导航结构 = docs/ 手稿目录的镜像：
//   一级组（Expansion）：开始 / 指南 / 组件
//   二级分类（Expansion 嵌套，"组件"组内）：基础/表单/布局/数据/高级
//   条目（菜单行）：切页；每个条目按 id 关联一个实况页（pages/<id>.js，
//   契约同 view/propbus：导出 meta + content），不再有占位页
// 开合态 openGroups 扁平单源（受控）；切页自动展开所在层级（navTo）。
// 语言切换写 langState、切页写 nav——均为 State 写 → 框架整树 reconcile，
// 读过这些态的节点随扩展翻转。
import {
    Root, View, Text, Button, Flex, ScrollView, State, Expansion, getWindowSize,
} from 'kwikui';
import { langState, switchLang, t } from './lang.js';
// 每个菜单项一个页面文件（与 view.js / propbus.js 同契约：meta + content）。
// 文件名按 NAV.id 镜像：连字符 id 用下划线文件名（layout-sys → layout_sys.js）。
import * as indexPage from './pages/index.js';
import * as statePage from './pages/state.js';
import * as propbusPage from './pages/propbus.js';
import * as channelPage from './pages/channel.js';
import * as animationPage from './pages/animation.js';
import * as themePage from './pages/theme.js';
import * as layoutSysPage from './pages/layout_sys.js';
import * as runtimePage from './pages/runtime.js';
import * as viewPage from './pages/view.js';
import * as textPage from './pages/text.js';
import * as buttonPage from './pages/button.js';
import * as imagePage from './pages/image.js';
import * as linePage from './pages/line.js';
import * as inputPage from './pages/input.js';
import * as textareaPage from './pages/textarea.js';
import * as checkboxPage from './pages/checkbox.js';
import * as switchPage from './pages/switch.js';
import * as radioButtonPage from './pages/radiobutton.js';
import * as sliderPage from './pages/slider.js';
import * as dropdownPage from './pages/dropdown.js';
import * as spinBoxPage from './pages/spinbox.js';
import * as dateTimePickerPage from './pages/datetimepicker.js';
import * as keyboardPage from './pages/keyboard.js';
import * as flexPage from './pages/flex.js';
import * as gridPage from './pages/grid.js';
import * as stackPage from './pages/stack.js';
import * as listPage from './pages/list.js';
import * as scrollViewPage from './pages/scrollview.js';
import * as lazyListPage from './pages/lazylist.js';
import * as stackIndexPage from './pages/stackindex.js';
import * as layerPage from './pages/layer.js';
import * as expansionPage from './pages/expansion.js';
import * as tablePage from './pages/table.js';
import * as tabsPage from './pages/tabs.js';
import * as chartPage from './pages/chart.js';
import * as gaugePage from './pages/gauge.js';
import * as treeMenuPage from './pages/treemenu.js';
import * as textViewPage from './pages/textview.js';
import * as progressBarPage from './pages/progressbar.js';
import * as progressRingPage from './pages/progressring.js';
import * as spinnerPage from './pages/spinner.js';

// 实况页模块（有 content() 的页面）；导航条目不依赖此表（NAV 单源）
const PAGES = [
    indexPage, statePage, propbusPage, channelPage, animationPage,
    themePage, layoutSysPage, runtimePage,
    viewPage, textPage, buttonPage, imagePage, linePage,
    inputPage, textareaPage, checkboxPage, switchPage, radioButtonPage,
    sliderPage, dropdownPage, spinBoxPage,
    dateTimePickerPage, keyboardPage,
    flexPage, gridPage, stackPage, listPage, scrollViewPage,
    lazyListPage, stackIndexPage, layerPage, expansionPage,
    tablePage, tabsPage, chartPage, gaugePage, treeMenuPage,
    textViewPage, progressBarPage, progressRingPage, spinnerPage,
];
const PAGE_BY_ID = Object.fromEntries(PAGES.map(p => [p.meta.id, p]));

// ── 导航层级词典 ──
const TOP_ORDER = ['docs', 'guide', 'comps'];
const TOP_GROUPS = {
    docs:  { zh: '开始', en: 'Start' },
    guide: { zh: '指南', en: 'Guide' },
    comps: { zh: '组件', en: 'Components' },
};
const CAT_ORDER = ['base', 'form', 'layout', 'data', 'progress', 'adv'];
const CATS = {
    base:   { zh: '基础', en: 'Basic' },
    form:   { zh: '表单', en: 'Form' },
    layout: { zh: '布局', en: 'Layout' },
    data:   { zh: '数据', en: 'Data' },
    progress: { zh: '进度与加载', en: 'Progress & Loading' },
    adv:     { zh: '高级', en: 'Advanced' },
};
// 二级分类 → 一级组归属（多级菜单的路由表）
const TOP_OF = Object.fromEntries(CAT_ORDER.map(c => [c, 'comps']));

// ── 导航注册表（左侧菜单单源）──
// 结合 docs/ 手稿目录建全量：guide 组 = 编号教程（docs/0-8），组件分类 =
// components/ 组件册。id 与实况页 meta.id 对齐的条目可点入实况页，
// 其余为占位（编写中提示 + doc 标注的手稿路径）。
const NAV = [
    { id: 'index', group: 'docs', title: { zh: '快速上手', en: 'Quick Start' }, doc: 'docs/0. 快速上手.md' },
    { id: 'state', group: 'guide', title: { zh: 'State 状态', en: 'State' }, doc: 'docs/2. State.md' },
    { id: 'propbus', group: 'guide', title: { zh: 'PropBus 属性总线', en: 'PropBus' }, doc: 'docs/3. PropBus.md' },
    { id: 'channel', group: 'guide', title: { zh: 'Channel 通道', en: 'Channel' }, doc: 'docs/4. Channel.md' },
    { id: 'animation', group: 'guide', title: { zh: '动画系统', en: 'Animation' }, doc: 'docs/5. 动画系统.md' },
    { id: 'theme', group: 'guide', title: { zh: '主题系统', en: 'Theme' }, doc: 'docs/6. 主题系统.md' },
    { id: 'layout-sys', group: 'guide', title: { zh: '布局系统', en: 'Layout System' }, doc: 'docs/7. 布局系统.md' },
    { id: 'runtime', group: 'guide', title: { zh: '运行时与生命周期', en: 'Runtime' }, doc: 'docs/8. 运行时与生命周期.md' },
    { id: 'view', group: 'base', title: { zh: 'View 视图', en: 'View' }, doc: 'docs/components/view.md' },
    { id: 'text', group: 'base', title: { zh: 'Text 文本', en: 'Text' }, doc: 'docs/components/text.md' },
    { id: 'button', group: 'base', title: { zh: 'Button 按钮', en: 'Button' }, doc: 'docs/components/button.md' },
    { id: 'image', group: 'base', title: { zh: 'Image 图片', en: 'Image' }, doc: 'docs/components/image.md' },
    { id: 'line', group: 'base', title: { zh: 'Line 线条', en: 'Line' }, doc: 'docs/components/line.md' },
    { id: 'input', group: 'form', title: { zh: 'Input 输入框', en: 'Input' }, doc: 'docs/components/input.md' },
    { id: 'textarea', group: 'form', title: { zh: 'TextArea 多行输入', en: 'TextArea' }, doc: 'docs/components/textarea.md' },
    { id: 'checkbox', group: 'form', title: { zh: 'Checkbox 复选框', en: 'Checkbox' }, doc: 'docs/components/checkbox.md' },
    { id: 'switch', group: 'form', title: { zh: 'Switch 开关', en: 'Switch' }, doc: 'docs/components/switch.md' },
    { id: 'radiobutton', group: 'form', title: { zh: 'RadioButton 单选', en: 'RadioButton' }, doc: 'docs/components/radiobutton.md' },
    { id: 'slider', group: 'form', title: { zh: 'Slider 滑杆', en: 'Slider' }, doc: 'docs/components/slider.md' },
    { id: 'dropdown', group: 'form', title: { zh: 'Dropdown 下拉选择', en: 'Dropdown' }, doc: 'docs/components/dropdown.md' },
    { id: 'spinbox', group: 'form', title: { zh: 'SpinBox 步进器', en: 'SpinBox' }, doc: 'docs/components/spinbox.md' },
    { id: 'datetimepicker', group: 'form', title: { zh: 'DateTimePicker 日期时间', en: 'DateTimePicker' }, doc: 'docs/components/datetimepicker.md' },
    { id: 'keyboard', group: 'form', title: { zh: 'Keyboard 软键盘', en: 'Keyboard' }, doc: 'docs/components/keyboard.md' },
    { id: 'flex', group: 'layout', title: { zh: 'Flex 弹性布局', en: 'Flex' }, doc: 'docs/components/flex.md' },
    { id: 'grid', group: 'layout', title: { zh: 'Grid 网格', en: 'Grid' }, doc: 'docs/components/grid.md' },
    { id: 'stack', group: 'layout', title: { zh: 'Stack 层叠', en: 'Stack' }, doc: 'docs/components/stack.md' },
    { id: 'list', group: 'layout', title: { zh: 'List 列表', en: 'List' }, doc: 'docs/components/list.md' },
    { id: 'scrollview', group: 'layout', title: { zh: 'ScrollView 滚动视图', en: 'ScrollView' }, doc: 'docs/components/scrollview.md' },
    { id: 'lazylist', group: 'layout', title: { zh: 'LazyList 惰性列表', en: 'LazyList' }, doc: 'docs/components/lazylist.md' },
    { id: 'stackindex', group: 'layout', title: { zh: 'StackIndex 页栈', en: 'StackIndex' }, doc: 'docs/components/stackindex.md' },
    { id: 'layer', group: 'layout', title: { zh: 'Layer 浮层', en: 'Layer' }, doc: 'docs/components/layer.md' },
    { id: 'expansion', group: 'layout', title: { zh: 'Expansion 折叠面板', en: 'Expansion' }, doc: 'docs/components/expansion.md' },
    { id: 'table', group: 'data', title: { zh: 'Table 表格', en: 'Table' }, doc: 'docs/components/table.md' },
    { id: 'tabs', group: 'data', title: { zh: 'Tabs 页签', en: 'Tabs' }, doc: 'docs/components/tabs.md' },
    { id: 'chart', group: 'data', title: { zh: 'Chart 图表', en: 'Chart' }, doc: 'docs/components/chart.md' },
    { id: 'gauge', group: 'data', title: { zh: 'Gauge 仪表盘', en: 'Gauge' }, doc: 'docs/components/gauge.md' },
    { id: 'treemenu', group: 'data', title: { zh: 'TreeMenu 树菜单', en: 'TreeMenu' }, doc: 'docs/components/treemenu.md' },
    { id: 'progressbar', group: 'progress', title: { zh: 'ProgressBar 进度条', en: 'ProgressBar' }, doc: 'docs/components/progressbar.md' },
    { id: 'progressring', group: 'progress', title: { zh: 'ProgressRing 进度环', en: 'ProgressRing' }, doc: 'docs/components/progressring.md' },
    { id: 'spinner', group: 'progress', title: { zh: 'Spinner 加载指示', en: 'Spinner' }, doc: 'docs/components/spinner.md' },
    { id: 'textview', group: 'adv', title: { zh: 'TextView 富文本', en: 'TextView' }, doc: 'docs/components/textview.md' },
    { id: 'g2d', group: 'adv', title: { zh: 'G2D 2D 绘制', en: 'G2D 2D' }, doc: 'docs/components/g2d.md' },
    { id: 'g3d', group: 'adv', title: { zh: 'G3D 3D 绘制', en: 'G3D 3D' }, doc: 'docs/components/extensions/g3d.md' },
    { id: 'video', group: 'adv', title: { zh: 'Video 视频', en: 'Video' }, doc: 'docs/components/extensions/video.md' },
];

const nav = new State({ page: 'index' });

// 开合态（受控扁平单源）：一级组 docs/guide/comps + 二级分类 base/.../adv。
// 初态展开"开始"（当前页所在）与"组件"（露出五个分类头骨架），分类全收起
// ——初屏完整呈现两级菜单结构，点开任意分类/条目后由 navTo 沿层级展开
const openGroups = new State({
    docs: true, guide: false, comps: true,
    base: false, form: false, layout: false, data: false, progress: false, adv: false,
});

// 窗口尺寸态：根声明 onWindowResize（框架仅根组件生效）→ 事件写 State →
// 整树 reconcile → 读它的节点随窗口缩放重排（响应式布局的框架正道）
const winSize = new State({ w: 0, h: 0 });

// ── 切页：写 nav + 沿层级路由展开所在一级组与二级分类（条目永远可达）──
const navTo = (id) => {
    nav.page = id;
    const it = NAV.find(n => n.id === id);
    if (!it) return;
    const top = TOP_OF[it.group] ?? it.group;    // 组件分类归 comps；docs/guide 即一级
    openGroups[top] = true;
    if (top === 'comps') openGroups[it.group] = true;
};

// ── 导航内容净宽：ScrollView 200 − 左 padding 16（条目行满宽高亮的基准）──
const NAV_INNER = 184;

// ── 菜单条目行（满宽行 + 文字随层级缩进；选中态整行浅蓝底 + 蓝字）──
const menuItem = (it, indent) => {
    const on = nav.page === it.id;
    return Flex({
        direction: 'row', alignItems: 'center',
        width: NAV_INNER, height: 28, borderRadius: 4,
        background: on ? '#E8EFFC' : 'transparent',
        padding: [0, 0, 0, indent], margin: [0, 0, 2, 0],
        onClick: () => navTo(it.id),
    }, [
        Text({ text: t(it.title), fontSize: 12.5, color: on ? '#2563EB' : '#475569' }),
    ]);
};

// ── 组/分类头行（children[0] 即头行契约）：箭头随开合翻转；层级经
// 缩进 + 字号 + 色深三重区分（一级 13/#0F172A，二级 11/#64748B）──
const groupHeader = (title, open, indent, fontSize, color) =>
    Flex({ direction: 'row', gap: 6, alignItems: 'center', height: 30, padding: [0, 0, 0, indent] }, [
        Text({ text: open ? '▼' : '▶', fontSize: 9, color: '#94A3B8' }),
        Text({ text: title, fontSize, color }),
    ]);

// ── 二级分类折叠（"组件"组内，Expansion 嵌套 Expansion）──
const catGroup = (cat) => {
    const open = !!openGroups[cat];
    return Expansion({
        open,
        onToggle: (e) => { openGroups[cat] = e.open; },
        headerHeight: 28, bodyGap: 0, margin: [0, 0, 2, 0],
    }, [
        groupHeader(t(CATS[cat]), open, 16, 11, '#64748B'),
        ...NAV.filter(n => n.group === cat).map(it => menuItem(it, 30)),
    ]);
};

// ── 一级组折叠：开始/指南直挂条目；组件 = 二级分类折叠嵌套 ──
const topGroup = (g) => {
    const open = !!openGroups[g];
    const body = g === 'comps'
        ? CAT_ORDER.map(catGroup)                                                    // 二级：分类折叠
        : NAV.filter(n => n.group === g).map(it => menuItem(it, 18));                // 直挂：一级条目
    return Expansion({
        open,
        onToggle: (e) => { openGroups[g] = e.open; },
        headerHeight: 30, bodyGap: 0, margin: [0, 0, 4, 0],
    }, [
        groupHeader(t(TOP_GROUPS[g]), open, 4, 13, '#0F172A'),
        ...body,
    ]);
};

const navPane = () => {
    return ScrollView({
        direction: 'vertical', width: 200,
        background: '#FFFFFF', padding: [12, 0, 16, 16], showScrollbar: true,
    }, [Flex({ direction: 'column' }, TOP_ORDER.map(topGroup))]);    // 双契约：① children 必须数组（裸对象被 parseNode 静默跳过变叶子）；② ScrollView 不流式排版（子节点按 x/y 定位），多内容包一层 Flex 作唯一内容子节点
};

// ── 占位页（导航条目对应的实况页尚未编写）：标题 + 编写中提示 + 手稿路径 ──
const pendingPage = (it) => Flex({ direction: 'column', gap: 8 }, [
    Text({ text: t(it.title), fontSize: 20, color: '#0F172A' }),
    Text({
        text: t({ zh: '该篇文档正在编写中——左侧导航标注了对应手稿，实况页将按手稿补齐。',
                  en: 'This page is not written yet — the sidebar points to the draft; the live page will follow it.' }),
        fontSize: 13, color: '#64748B',
    }),
    Text({ text: `${t({ zh: '手稿', en: 'Draft' })}: ${it.doc}`, fontSize: 12, color: '#94A3B8' }),
]);

// ── 内容区：页面自治组装——content() 由各页定义并负责全部卡片
// （标题/描述/实况卡/代码卡/属性表的顺序与尺寸都是页面自己的事）；
// main 只负责框架壳与净宽下传，实况页缺失时落占位页 ──
// innerW = 内容区净宽（contentInner），下传各卡片工厂随窗口缩放
const pageBody = (innerW) => {
    const page = PAGE_BY_ID[nav.page];
    if (page && page.content) return page.content(innerW);
    const it = NAV.find(n => n.id === nav.page);
    return it ? pendingPage(it)
              : [Text({ text: 'page not found', fontSize: 14, color: '#EF4444' })];
};

const contentPane = () => {
    return ScrollView({
        direction: 'vertical', width: winSize.w - 200,    // 读 winSize（与根布局同源），不直查窗口
        background: '#F5F7FB', padding: [18, 12, 32, 28], showScrollbar: true,
    }, [Flex({ direction: 'column' }, pageBody(contentInner()))]);    // 同 navPane：children 数组 + Flex 内容节点双契约；净宽下传 pageBody 全量卡片
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
            winSize.w = e.width; winSize.h = e.height;
        },

    }, View({ width: W, height: H, background: '#F5F7FB' }, [
        header(W),
        Flex({ direction: 'row', width: W, height: H - 52 }, [
            navPane(),
            contentPane(),
        ]),
    ]));
};
