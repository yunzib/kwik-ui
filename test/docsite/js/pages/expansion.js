// ── Expansion 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/expansion.md。
// 四节结构逐行镜像 test/ui/expansion.js（上位机验证可用的四景 demo）：
// ① 非受控双卡（defaultOpen 开/关各一）② 受控卡 + onToggle 事件流
// ③ 互斥手风琴（ScrollView 组，与非受控实例混用）④ 头行组合自由度。
// demo 侧改动必须与本页演示同步（reverse 也一样）。
import { View, Flex, Text, Button, ScrollView, State, Expansion, Image } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'expansion',
    group: 'layout',
    title: { zh: 'Expansion 折叠面板', en: 'Expansion' },
    desc: {
        zh: '一档开合容器：children[0] 就是头行，标题、箭头、徽标都由调用方自由组合；'
            + '非受控模式用 defaultOpen（body 隐藏保活），受控模式用 open + onToggle（body 可条件拼装）；'
            + '互斥手风琴 = 调用方 State 一行判别，不自建数据模型。',
        en: 'Single-section collapsible container: children[0] is the header, fully composed by the caller. '
            + 'Uncontrolled mode uses defaultOpen (body stays alive hidden); controlled uses open + onToggle '
            + '(body may be conditionally composed). The exclusive accordion is one State line in the caller.',
    },
    props: [
        { title: { zh: '头行契约与开合模式', en: 'Header contract & mode' }, rows: [
            ['children[0]', 'View 组合', '-', { zh: '头行内容显式提供（零内建装配）', en: 'Header content, explicit (no built-ins)' }],
            ['open', 'boolean', 'false', { zh: '受控开合（键存在即受控）', en: 'Controlled open (presence = controlled)' }],
            ['defaultOpen', 'boolean', 'false', { zh: '非受控初值（重喂不回灌）', en: 'Uncontrolled initial value' }],
            ['onToggle', '(e) => {}', '-', { zh: '开合通报 e={open}', en: 'Toggle callback e={open}' }],
        ] },
        { title: { zh: '外观', en: 'Appearance' }, rows: [
            ['headerHeight', 'number', '28', { zh: '头行带高度', en: 'Header band height' }],
            ['headerBackground', '#rrggbb', '透明', { zh: '头行带底色', en: 'Header band background' }],
            ['bodyGap', 'number', '0', { zh: 'body 子项间距', en: 'Body gap' }],
        ] },
    ],
};

const L = {
    s1: { zh: '非受控（defaultOpen，点击头行翻转；箭头 = onToggle 镜像 State）', en: 'Uncontrolled (defaultOpen; arrow mirrored via onToggle)' },
    s2: { zh: '受控（open: State + onToggle 回写，body 条件拼装）', en: 'Controlled (open + onToggle, conditional body)' },
    s3: { zh: '互斥手风琴（ScrollView 组：点谁谁开）', en: 'Exclusive accordion (ScrollView group)' },
    s4: { zh: '头行组合自由度（children[0] 显式提供）', en: 'Header composition freedom' },
    ncOpenT: { zh: '默认展开（隐藏保活）', en: 'Default open (kept alive hidden)' },
    ncBody1: { zh: 'body 子级参数一：收起后仅翻 visible——组件内状态保留（Text 排版缓存不丢），',
               en: 'Body note 1: collapsing only flips visible — inner state and text layout caches persist,' },
    ncBody2: { zh: '体高度按 flex 引擎排除不可见子级计（flex_layout.cpp:69/152）。',
               en: 'and flex height skips invisible children (flex_layout.cpp:69/152).' },
    ncClosedT: { zh: '默认收起（点开试试，箭头静态即不镜像）', en: 'Default closed (tap to open; static arrow = no mirroring)' },
    ncClosedBody: { zh: '收起态的 body（点头行展开后可见）', en: 'Collapsed body (visible after opening the header)' },
    ctlT: { zh: '受控卡（State 当前 open=', en: 'Controlled card (State open=' },
    ctlBody: { zh: '此处内容随 State 切换真实挂/卸（条件拼装）。', en: 'This body composes/decomposes with State (conditional composition).' },
    ctlBtn: { zh: '编程展开/收起（改 State 即可）', en: 'Open/close programmatically (just write State)' },
    logTitle: { zh: '事件流（onToggle 日志）', en: 'Event stream (onToggle log)' },
    logEmpty: { zh: '（暂无）', en: '(none)' },
    logLine: { zh: 'onToggle → open=', en: 'onToggle → open=' },
    accPrefix: { zh: '组', en: 'Group' },
    accAT: { zh: '导航分组（展开）', en: 'Nav group (open)' },
    accBodyA: { zh: '受控开合 + 收放全链重排；头行内容显式提供，与演示一致', en: 'Controlled toggling with full relayout; header composition matches the code sample' },
    accBT: { zh: '目录树分组（半展开）', en: 'Tree group (half open)' },
    accBodyB: { zh: '互斥逻辑 = 调用方 State 一行（点谁谁开）；Expansion 只管开合', en: 'The exclusivity is one State line; Expansion only owns the toggle' },
    accCT: { zh: 'FAQ 分组', en: 'FAQ group' },
    accBodyC: { zh: 'body 内放任意 View 组合——行定制自由度是它相对集成树的核心优势', en: 'The body takes any View composition — the trade-off vs. integrated trees' },
    accDT: { zh: '组 D — 非受控（与互斥组共存）', en: 'Group D — uncontrolled (coexists with the exclusive group)' },
    accDBody: { zh: '两种模式可混用在不同实例上。', en: 'Both modes mix freely across instances.' },
    scrollNote: { zh: '后续内容随折叠上下收放（scroll 引擎自动重算内容高）', en: 'Content reflows as groups toggle (scroll extent recomputed automatically)' },
    plusT: { zh: '加减号头行（箭头即普通 Text 元素）', en: 'Plus-sign header (arrow is a plain Text)' },
    plusB: { zh: '头行内容零内建——箭头字形/图标/徽标都是 children[0] 里的普通元素组合。', en: 'No built-in header marks — glyphs, icons and badges are ordinary elements in children[0].' },
    iconT: { zh: '图标头行（Image/SVG 随意换）', en: 'Icon header (Image/SVG are free to swap)' },
    iconB: { zh: '图标样式归元素自身（src/尺寸/主题换资产），组件不再代理。', en: 'Icon styling lives on the element itself (src, size, assets) — never proxied by the component.' },
    badgeT: { zh: '徽标头行（flexGrow 撑开 + State 计数）', en: 'Badged header (flexGrow spacer + State count)' },
    badgeBtn: { zh: '计数 +1（徽标吃 State）', en: '+1 (badge reads State)' },
    plainT: { zh: '纯文字头行（无箭头——头行里不放就是了）', en: 'Text-only header (no arrow — just leave it out)' },
    plainB: { zh: '标签分组标题样式，头行元素自由裁剪。', en: 'Group-label style; trim the header composition freely.' },
    hint1: { zh: '非受控：defaultOpen 起值内部自持，JS 重喂同构 props 不回灌内部态；接 onToggle 镜像 State 才有箭头联动（内部态不回灌，两者可叠加）。', en: 'Uncontrolled: defaultOpen seeds internal state; arrow sync only when onToggle mirrors State (internal state is not fed back — both compose).' },
    hint2: { zh: '受控：open 即当前态（键存在即受控）——onToggle 只通报 e={open}，开合由调用方 State 回灌后下一帧应用；body 可条件拼装（真实挂/卸）。', en: 'Controlled: open IS the state (presence = controlled) — onToggle just reports e={open}; the toggle applies next frame after your State write, and the body may be conditionally composed.' },
    hint3: { zh: '互斥手风琴不自建数据模型：open 是表达式、onToggle 里一行判别；收放改测量高，ScrollView 内容高随之重算；组内也可混用非受控实例。', en: 'The exclusive accordion keeps no internal model: open is an expression, onToggle one line; toggling changes measured height, so the ScrollView extent follows; uncontrolled instances may coexist.' },
    hint4: { zh: '头行样式不经 props——字形、图标、徽标全是 children[0] 元素自身属性；body 内 Button 点击即普通 body 事件（不沾头行开合语义）。', en: 'Header styling lives on the children[0] elements themselves, not props; a Button in the body behaves as a normal body event (no header-toggle semantics).' },
};

// ── 各节 State（命名对齐 test/ui demo：nc1 镜像箭头 / ctl 受控 / logs 事件流
// / badge 徽标 / acc 互斥；ACCESSED via State 原型，rebuild 随读值重判）──
const nc1 = new State({ open: true });        // ① 镜像 State（仅驱动箭头文本）
const ctl = new State({ open: false });       // ② 受控卡
const logs = new State({ lines: [] });
const pushLog = (msg) => {
    logs.lines = [...logs.lines, msg];        // 展开重赋值（不 mutate——触发 rebuild 的正解）
};
const badge = new State({ count: 3 });        // ④ 徽标计数
const acc = new State({ open: 'A' });         // ③ 互斥单开：同 key 才展开

// ── 头行内容 helper（children[0] 即头行；照 test/ui demo 逐字）──
// 图标/标题/徽标即普通元素组合；箭头联动 = State 条件表达式（受控或镜像）
const hd = (children, h = 28) =>
    Flex({ direction: 'row', gap: 6, alignItems: 'center', height: h, padding: [0, 10, 0, 10] }, children);

// ── ③ 互斥条目（照 demo accItem 逐字：open 是表达式 + 一行判别）──
const accItem = (key, title, hint) =>
    Expansion({
        id: 'acc_' + key,
        open: acc.open === key,               // State 读随重扩展翻转
        onToggle: (e) => { if (e.open) acc.open = key; },    // 互斥：点谁谁开
        headerHeight: 30, headerBackground: '#EEF1F6',
        margin: [0, 0, 6, 0],
    }, [
        hd([
            Text({ text: acc.open === key ? '▼' : '▶', fontSize: 10, color: '#64748B' }),
            Text({ text: t(L.accPrefix) + ' ' + key + ' — ' + title, fontSize: 12, color: '#334155' }),
        ], 30),
        Text({ text: hint, fontSize: 12, color: '#64748B', margin: [4, 12, 8, 12] }),
    ]);

// ── ① 非受控双卡 ──
const demo1 = () => [
    Flex({ direction: 'column', gap: 12 }, [
        Expansion({
            id: 'nc_open', defaultOpen: true, headerBackground: '#E3F2FD',
            onToggle: (e) => { nc1.open = e.open; },    // 镜像：重渲染驱动箭头，内部态不回灌
        }, [
            hd([
                Text({ text: nc1.open ? '▼' : '▶', fontSize: 10, color: '#1976D2' }),
                Text({ text: t(L.ncOpenT), fontSize: 12, color: '#1565C0' }),
            ]),
            Text({ text: t(L.ncBody1), fontSize: 13, color: '#444444' }),
            Text({ text: t(L.ncBody2), fontSize: 13, color: '#444444' }),
        ]),
        Expansion({
            id: 'nc_closed', defaultOpen: false, headerBackground: '#E3F2FD',
        }, [
            hd([
                Text({ text: '▶', fontSize: 10, color: '#1976D2' }),
                Text({ text: t(L.ncClosedT), fontSize: 12, color: '#1565C0' }),
            ]),
            Text({ text: t(L.ncClosedBody), fontSize: 13, color: '#444444' }),
        ]),
    ]),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

// ── ② 受控卡 + 事件流 ──
const demo2 = () => [
    Flex({ direction: 'column', gap: 8 }, [
        Expansion({
            id: 'ctl_card',
            open: ctl.open,
            onToggle: (e) => { ctl.open = e.open; pushLog(t(L.logLine) + e.open); },
            headerBackground: '#E8F5E9',
        }, [
            hd([
                Text({ text: ctl.open ? '▼' : '▶', fontSize: 10, color: '#2E7D32' }),
                Text({ text: t(L.ctlT) + String(ctl.open) + ')', fontSize: 12, color: '#2E7D32' }),
            ]),
            ...(ctl.open ? [    // 受控条件拼装：真实挂/卸（状态归调用方）
                Text({ text: t(L.ctlBody), fontSize: 13, color: '#444444' }),
                Button({
                    text: t(L.ctlBtn),
                    width: 220, height: 28, fontSize: 12,
                    background: '#2E7D32', color: '#FFFFFF', borderRadius: 6,
                    margin: [6, 0, 0, 0],
                    onClick: () => { ctl.open = !ctl.open; },
                }),
            ] : []),
        ]),
        Text({ text: t(L.logTitle), fontSize: 13, color: '#888888', margin: [12, 0, 4, 0] }),
        Flex({ direction: 'column', background: '#FFFFFF', borderRadius: 8, padding: 8 }, [
            ...logs.lines.slice(-6).map((s, i) => Text({ text: s, fontSize: 11, color: '#2E7D32', key: 'lg' + i })),
            ...(logs.lines.length ? [] : [Text({ text: t(L.logEmpty), fontSize: 11, color: '#BBBBBB' })]),
        ]),
    ]),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

// ── ③ 互斥手风琴（ScrollView 组 + 非受控混用）──
const demo3 = () => [
    ScrollView({
        direction: 'vertical', width: 560, height: 620,
        background: '#FFFFFF', borderRadius: 10, padding: [10, 0, 10, 10],
        showScrollbar: true,
    }, [
        Flex({ direction: 'column' }, [
            accItem('A', t(L.accAT), t(L.accBodyA)),
            accItem('B', t(L.accBT), t(L.accBodyB)),
            accItem('C', t(L.accCT), t(L.accBodyC)),
            Expansion({
                id: 'acc_free',
                defaultOpen: false,
                headerHeight: 30, headerBackground: '#FFF3E0',
                margin: [0, 0, 6, 0],
            }, [
                hd([
                    Text({ text: '▶', fontSize: 10, color: '#EF6C00' }),
                    Text({ text: t(L.accDT), fontSize: 12, color: '#EF6C00' }),
                ], 30),
                Text({ text: t(L.accDBody), fontSize: 12, color: '#5D4037', margin: [4, 12, 8, 12] }),
            ]),
            Text({ text: t(L.scrollNote), fontSize: 12, color: '#999999', margin: [8, 12] }),
        ]),
    ]),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

// ── ④ 头行组合自由度（字形/图标/徽标/纯文字）──
const demo4 = () => [
    Flex({ direction: 'column', gap: 12 }, [
        Expansion({ id: 'hd_plus', defaultOpen: true, headerBackground: '#EDE7F6' }, [
            hd([
                Text({ text: '+', fontSize: 14, color: '#5E35B1' }),
                Text({ text: t(L.plusT), fontSize: 12, color: '#4527A0' }),
            ]),
            Text({ text: t(L.plusB), fontSize: 12, color: '#444444' }),
        ]),
        Expansion({ id: 'hd_icon', defaultOpen: true, headerBackground: '#E0F2F1' }, [
            hd([
                Image({ src: '../../test/image/menu.svg', width: 14, height: 14 }),
                Text({ text: t(L.iconT), fontSize: 12, color: '#00695C' }),
            ]),
            Text({ text: t(L.iconB), fontSize: 12, color: '#444444' }),
        ]),
        Expansion({ id: 'hd_badge', defaultOpen: true, headerBackground: '#FFF8E1' }, [
            hd([
                Text({ text: t(L.badgeT), fontSize: 12, color: '#F57F17' }),
                View({ flexGrow: 1 }),
                Text({ text: String(badge.count), fontSize: 11, color: '#795548', background: '#FCE38A', borderRadius: 8, padding: [1, 7, 1, 7] }),
            ]),
            Button({
                text: t(L.badgeBtn), width: 160, height: 26, fontSize: 12,
                background: '#F57F17', color: '#FFFFFF', borderRadius: 6,
                onClick: () => { badge.count++; },
            }),
        ]),
        Expansion({ id: 'hd_plain', defaultOpen: true, headerBackground: '#FCE4EC' }, [
            hd([Text({ text: t(L.plainT), fontSize: 12, color: '#AD1457' })]),
            Text({ text: t(L.plainB), fontSize: 12, color: '#444444' }),
        ]),
    ]),
    Text({ text: t(L.hint4), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

// 代码卡（照 demo 样式同源；改动演示同步这里）
const C1 = `// ① 非受控：defaultOpen 起值，内部自持；onToggle 镜像 → 箭头联动
const mirror = new State({ open: true });

Expansion({ defaultOpen: true, headerBackground: '#E3F2FD',
            onToggle: (e) => { mirror.open = e.open; } }, [
    Flex({ direction: 'row', gap: 6, alignItems: 'center',
           height: 28, padding: [0, 10, 0, 10] }, [      // ← children[0] = 头行
        Text({ text: mirror.open ? '▼' : '▶', fontSize: 10 }),
        Text({ text: '默认展开（隐藏保活）' }),
    ]),
    Text({ text: 'body 子级：收起后仅翻 visible，状态与排版缓存保留' }),
]);

// 不接 onToggle → 箭头写死 ▶（不联动）
Expansion({ defaultOpen: false, headerBackground: '#E3F2FD' }, [
    Flex({ direction: 'row', gap: 6, alignItems: 'center',
           height: 28, padding: [0, 10, 0, 10] }, [Text({ text: '▶' }), Text({ text: '默认收起' })]),
    Text({ text: '收起态的 body（点头行展开后可见）' }),
]);`;

const C2 = `// ② 受控：状态归调用方；onToggle 只通报，开合靠 State 回灌
const ctl = new State({ open: false });
const logs = new State({ lines: [] });

Expansion({ open: ctl.open,
            onToggle: (e) => { ctl.open = e.open;
                               logs.lines = [...logs.lines, 'onToggle → open=' + e.open]; },
            headerBackground: '#E8F5E9' }, [
    Flex({ direction: 'row', gap: 6, alignItems: 'center',
           height: 28, padding: [0, 10, 0, 10] }, [
        Text({ text: ctl.open ? '▼' : '▶' }),
        Text({ text: '受控卡（State 当前 open=' + ctl.open + '）' }),
    ]),
    ...(ctl.open ? [                             // 条件拼装：真实挂/卸
        Text({ text: '内容随 State 切换' }),
        Button({ text: '编程展开/收起', onClick: () => { ctl.open = !ctl.open; } }),
    ] : []),
]);

// 事件流卡：logs.lines.slice(-6) 展示最近通报
Flex({ direction: 'column', background: '#FFFFFF', borderRadius: 8, padding: 8 },
     logs.lines.slice(-6).map((s) => Text({ text: s })));`;

const C3 = `// ③ 互斥手风琴：open 是表达式 + onToggle 一行判别
const acc = new State({ open: 'A' });    // 单开（互斥）：同 key 才展开

const accItem = (key, title, hint) => Expansion({
    id: 'acc_' + key,
    open: acc.open === key,                          // State 读随重扩展翻转
    onToggle: (e) => { if (e.open) acc.open = key; },  // 点谁谁开
    headerHeight: 30, headerBackground: '#EEF1F6',
}, [
    Flex({ direction: 'row', gap: 6, alignItems: 'center',
           height: 30, padding: [0, 10, 0, 10] }, [
        Text({ text: acc.open === key ? '▼' : '▶' }),
        Text({ text: '组 ' + key + ' — ' + title }),
    ]),
    Text({ text: hint, margin: [4, 12, 8, 12] }),
]);

// ScrollView 组：收放改测量高，内容高随滚动引擎重算
ScrollView({ direction: 'vertical', width: 560, height: 620 },
    [ Flex({ direction: 'column' }, [
        accItem('A', '导航分组', '受控开合 + 收放全链重排'),
        accItem('B', '目录树分组', '互斥 = 调用方 State 一行'),
        accItem('C', 'FAQ 分组', 'body 内放任意 View 组合'),
        Expansion({ id: 'acc_free', defaultOpen: false,      // 非受控混用
                    headerHeight: 30, headerBackground: '#FFF3E0' }, [...]),
    ]) ]);`;

const C4 = `// ④ 头行零内建：字形/图标/徽标/纯文字——全在 children[0] 里自由拼
Expansion({ id: 'hd_plus', defaultOpen: true, headerBackground: '#EDE7F6' }, [
    Flex({ direction: 'row', gap: 6, alignItems: 'center',
           height: 28, padding: [0, 10, 0, 10] }, [
        Text({ text: '+', fontSize: 14 }),          // 加减号…随便换字形
        Text({ text: '加减号头行' }),
    ]),
    Text({ text: '头行内容零内建' }),
]);

Expansion({ id: 'hd_icon', defaultOpen: true, headerBackground: '#E0F2F1' }, [
    Flex({ direction: 'row', gap: 6, alignItems: 'center', height: 28 }, [
        Image({ src: '../../test/image/menu.svg', width: 14, height: 14 }),
        Text({ text: '图标头行（Image/SVG 随意换）' }),
    ]),
    Text({ text: '图标样式归元素自身' }),
]);

Expansion({ id: 'hd_badge', defaultOpen: true, headerBackground: '#FFF8E1' }, [
    Flex({ direction: 'row', gap: 6, alignItems: 'center', height: 28 }, [
        Text({ text: '徽标头行' }),
        View({ flexGrow: 1 }),                      // 弹性撑开
        Text({ text: String(badge.count), background: '#FCE38A', borderRadius: 8 }),
    ]),
    Button({ text: '计数 +1', onClick: () => { badge.count++; } }),   // body 普通事件
]);

Expansion({ id: 'hd_plain', defaultOpen: true, headerBackground: '#FCE4EC' }, [
    Flex({ direction: 'row', gap: 6, alignItems: 'center', height: 28 },
         [Text({ text: '纯文字头行（无箭头）' })]),
    Text({ text: '标签分组标题样式' }),
]);`;

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    ...section(t(L.s3), demo3(), C3, innerW),
    ...section(t(L.s4), demo4(), C4, innerW),
    propsTable(t, meta.props, innerW),
];
