// ── G2D 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/g2d.md。
// 三节结构镜像 test/ui/g2d.js 验证 demo（单画布 API 画廊拆三节收进页面：
// 矩形三连 / 路径与曲线 / 圆弧·椭圆·状态栈）。demo 侧改动必须同步本页
// （reverse 也是）。
// 注意：绘制命令在 content() 构建期发出（eager 实例 + findById 回填），
// 整树 rebuild 重放同套命令——幂等重绘，不累积。
import { Text, G2D } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'g2d',
    group: 'adv',
    title: { zh: 'G2D 2D 绘制', en: 'G2D 2D' },
    desc: {
        zh: 'Canvas 风格命令式 2D 绘制（对标 Web CanvasRenderingContext2D 常用子集）：'
            + '工厂返回绘制面后直接调用绘制方法——矩形 / 路径 / 贝塞尔 / 圆弧 / 椭圆，'
            + '配 save/restore 状态栈与样式属性；经插件式注册加入 kwikui 导出。',
        en: 'Canvas-style imperative 2D drawing (a Web CanvasRenderingContext2D subset): '
            + 'the factory returns a draw surface you call methods on — rects, paths, '
            + 'beziers, arcs and ellipses with a save/restore state stack.',
    },
    props: [
        { title: { zh: '画布与样式', en: 'Canvas & Style' }, rows: [
            ['width / height', 'number', '必填', { zh: '画布逻辑宽 / 高', en: 'Logical canvas size' }],
            ['fillStyle / strokeStyle', '#rrggbb | rgba()', '#000000', { zh: '填充 / 描边色（属性赋值或 setFillStyle / setStrokeStyle）', en: 'Fill / stroke color (property or setters)' }],
            ['lineWidth / globalAlpha', 'number', '1 / 1.0', { zh: '线宽 / 全局透明度（setLineWidth / setGlobalAlpha）', en: 'Line width / global alpha (setters)' }],
        ] },
    ],
};

const L = {
    s1: { zh: '矩形三连：fillRect / strokeRect / clearRect', en: 'Rects: fillRect / strokeRect / clearRect' },
    s2: { zh: '路径与曲线：beginPath → moveTo / lineTo / 贝塞尔', en: 'Paths & curves: beginPath → moveTo / lineTo / beziers' },
    s3: { zh: '圆弧·椭圆·状态栈：arc / ellipse + save / restore', en: 'Arcs, ellipses & state stack' },
    hint1: { zh: 'fillRect 实心块、strokeRect 只描边（setLineWidth 控线宽）、clearRect 挖空成透明'
               + '——颜色支持 #rgb / #rrggbb / rgba() 三种写法。',
            en: 'fillRect for solids, strokeRect for outlines (setLineWidth), clearRect punches '
                + 'transparent holes — colors accept #rgb / #rrggbb / rgba().' },
    hint2: { zh: '路径三步：beginPath 起稿 → moveTo/lineTo/quadraticCurveTo/bezierCurveTo 描线 →'
               + ' closePath 闭合后 fill 填充或直接 stroke 描线。',
            en: 'Three steps: beginPath, then moveTo/lineTo/quadraticCurveTo/bezierCurveTo, '
                + 'then closePath + fill, or stroke directly.' },
    hint3: { zh: 'arc(cx,cy,r,start,end,ccw) 圆弧、ellipse 多 rx/ry/rotation 两参；save/restore '
               + '是样式状态栈——save 后改透明度叠画两块，restore 取回旧样式。',
            en: 'arc(cx,cy,r,start,end,ccw) draws circles; ellipse adds rx/ry/rotation. '
                + 'save/restore is a style stack — drop alpha, overdraw, then restore.' },
};

// ── ① 矩形三连（demo 首两节 + clearRect 合并镜像）──
const demo1 = () => {
    const c = G2D({ id: 'ds-g2d-rect', width: 400, height: 200 });
    c.setFillStyle('#E74C3C');            c.fillRect(10, 10, 120, 80);
    c.setFillStyle('#F39C12');            c.fillRect(140, 10, 120, 80);
    c.setFillStyle('rgba(241,196,15,1)'); c.fillRect(270, 10, 120, 80);
    c.setStrokeStyle('#2ECC71'); c.setLineWidth(4); c.strokeRect(10, 110, 120, 70);
    c.setStrokeStyle('#3498DB'); c.setLineWidth(2); c.strokeRect(140, 110, 120, 70);
    c.setStrokeStyle('#9B59B6'); c.setLineWidth(1); c.strokeRect(270, 110, 120, 70);
    c.clearRect(30, 20, 60, 50);          // 从红块挖空
    c.clearRect(290, 30, 80, 40);         // 从黄块挖空
    return [
        c,
        Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
    ];
};

// ── ② 路径与曲线（demo ③④⑤ 镜像：填充三角 / 二次抛物线 / 三次 S 形）──
const demo2 = () => {
    const c = G2D({ id: 'ds-g2d-path', width: 400, height: 130 });
    c.setFillStyle('#E74C3C');
    c.beginPath();
    c.moveTo(20, 20);  c.lineTo(90, 115);  c.lineTo(20, 115);
    c.closePath();     c.fill();                       // 填充三角
    c.setStrokeStyle('#2ECC71'); c.setLineWidth(3);
    c.beginPath();     c.moveTo(130, 115);
    c.quadraticCurveTo(180, 30, 230, 115);  c.stroke(); // 二次贝塞尔（抛物线）
    c.setStrokeStyle('#3498DB'); c.setLineWidth(3);
    c.beginPath();     c.moveTo(260, 115);
    c.bezierCurveTo(290, 30, 330, 130, 360, 70);  c.stroke();   // 三次贝塞尔（S 形）
    return [
        c,
        Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
    ];
};

// ── ③ 圆弧·椭圆·状态栈（demo ⑥⑦⑧ 镜像）──
const demo3 = () => {
    const c = G2D({ id: 'ds-g2d-arc', width: 400, height: 140 });
    c.setFillStyle('#8E44AD');
    c.beginPath(); c.ellipse(70, 70, 45, 20, 0.5, 0, Math.PI * 2, false); c.fill();   // 旋转椭圆
    c.setFillStyle('#1ABC9C');
    c.beginPath(); c.arc(220, 70, 30, 0, Math.PI * 2, false); c.fill();               // 实心圆
    c.setStrokeStyle('#E67E22'); c.setLineWidth(4);
    c.beginPath(); c.arc(300, 95, 28, 0, Math.PI, false); c.stroke();                 // 上半弧
    c.save();
    c.setGlobalAlpha(0.4);
    c.setFillStyle('#E74C3C'); c.fillRect(330, 20, 50, 50);                           // 半透明叠画
    c.setFillStyle('#3498DB'); c.fillRect(345, 40, 50, 50);
    c.restore();
    c.setFillStyle('#8E44AD');                                                        // restore 取回紫色
    c.beginPath(); c.arc(385, 15, 8, 0, Math.PI * 2, false); c.fill();
    return [
        c,
        Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
    ];
};

// 代码卡（与上方演示逐行同源；改动演示同步这里）
const C1 = `const c = G2D({ id: 'ds-g2d-rect', width: 400, height: 200 });
c.setFillStyle('#E74C3C');            c.fillRect(10, 10, 120, 80);
c.setFillStyle('#F39C12');            c.fillRect(140, 10, 120, 80);
c.setFillStyle('rgba(241,196,15,1)'); c.fillRect(270, 10, 120, 80);

c.setStrokeStyle('#2ECC71'); c.setLineWidth(4); c.strokeRect(10, 110, 120, 70);
c.setStrokeStyle('#3498DB'); c.setLineWidth(2); c.strokeRect(140, 110, 120, 70);
c.setStrokeStyle('#9B59B6'); c.setLineWidth(1); c.strokeRect(270, 110, 120, 70);

c.clearRect(30, 20, 60, 50);    // 挖空 → 透明
c.clearRect(290, 30, 80, 40);`;

const C2 = `const c = G2D({ id: 'ds-g2d-path', width: 400, height: 130 });

// 填充三角：closePath 闭合后 fill
c.setFillStyle('#E74C3C');
c.beginPath();
c.moveTo(20, 20); c.lineTo(90, 115); c.lineTo(20, 115);
c.closePath();    c.fill();

// 二次贝塞尔（抛物线）：控制点在顶
c.setStrokeStyle('#2ECC71'); c.setLineWidth(3);
c.beginPath(); c.moveTo(130, 115);
c.quadraticCurveTo(180, 30, 230, 115); c.stroke();

// 三次贝塞尔（S 形）：两控制点上下拉弯
c.setStrokeStyle('#3498DB'); c.setLineWidth(3);
c.beginPath(); c.moveTo(260, 115);
c.bezierCurveTo(290, 30, 330, 130, 360, 70); c.stroke();`;

const C3 = `const c = G2D({ id: 'ds-g2d-arc', width: 400, height: 140 });

// 椭圆：ellipse(cx, cy, rx, ry, rotation, start, end, ccw)
c.setFillStyle('#8E44AD');
c.beginPath(); c.ellipse(70, 70, 45, 20, 0.5, 0, Math.PI * 2, false); c.fill();

// 圆：arc(cx, cy, r, start, end, ccw)；半弧改角度区间即成
c.setFillStyle('#1ABC9C');
c.beginPath(); c.arc(220, 70, 30, 0, Math.PI * 2, false); c.fill();
c.setStrokeStyle('#E67E22'); c.setLineWidth(4);
c.beginPath(); c.arc(300, 95, 28, 0, Math.PI, false); c.stroke();

// 状态栈：save → 降透明度叠画 → restore 取回旧样式
c.save();
c.setGlobalAlpha(0.4);
c.setFillStyle('#E74C3C'); c.fillRect(330, 20, 50, 50);
c.setFillStyle('#3498DB'); c.fillRect(345, 40, 50, 50);
c.restore();
c.setFillStyle('#8E44AD');    // restore 后仍是紫色（save 时快照）
c.beginPath(); c.arc(385, 15, 8, 0, Math.PI * 2, false); c.fill();`;

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    ...section(t(L.s3), demo3(), C3, innerW),
    propsTable(t, meta.props, innerW),
];
