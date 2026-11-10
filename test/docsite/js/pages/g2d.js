// ── G2D 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/g2d.md。
// 五节结构镜像 test/ui/g2d.js 验证 demo（12 画布 API 画廊收敛为五节：
// 矩形三连 / 路径与曲线 / 圆弧·椭圆·状态栈 / 凹多边形 ear clipping /
// 文本·渐变·变换）。demo 侧改动必须同步本页（reverse 也是）。
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
            + '工厂返回绘制面后直接调用绘制方法——矩形 / 路径 / 贝塞尔 / 圆弧 / 椭圆 /'
            + ' fillText 文本 / 渐变画刷 / translate·rotate·scale 变换，'
            + '凹多边形 ear clipping 填充，配 save/restore 状态栈与样式属性；'
            + '经插件式注册加入 kwikui 导出。',
        en: 'Canvas-style imperative 2D drawing (a Web CanvasRenderingContext2D subset): '
            + 'the factory returns a draw surface you call methods on — rects, paths, '
            + 'beziers, arcs, ellipses, fillText, gradient brushes and '
            + 'translate/rotate/scale transforms, with concave-polygon ear-clipping fill '
            + 'and a save/restore state stack.',
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
    s4: { zh: '凹多边形填充（ear clipping）：五角星 / 闪电 / 箭头', en: 'Concave polygons (ear clipping): star / bolt / arrow' },
    hint4: { zh: 'fill() 内部按 ear clipping 剖分，凹多边形一次 fill 正确成型——五角星、闪电'
               + '这类强凹轮廓不会像形心三角扇那样被中孔失真覆盖；描边不受影响。',
            en: 'fill() tessellates with ear clipping, so strongly concave outlines '
                + '(star, bolt, arrow) fill correctly in one call.' },
    s5: { zh: '文本·渐变·变换：fillText / createLinear·RadialGradient / translate·rotate·scale',
          en: 'Text, gradients & transforms' },
    hint5: { zh: 'fillText(text,x,y,fontSize?) 走与 Text 组件同一条 glyph 管线；渐变对象传给 '
               + 'setFillStyle 后 fillRect 走真渐变（路径填充取起点色兜底）；translate/rotate/'
               + 'scale 录制为命令、重放期进 Graphics 状态栈，save/restore 同时弹压样式与变换。',
            en: 'fillText uses the same glyph pipeline as Text; a gradient object handed to '
                + 'setFillStyle turns fillRect into a true gradient (paths fall back to the '
                + 'start color); translate/rotate/scale are recorded and replayed through the '
                + 'Graphics state stack — save/restore pops styles and transforms together.' },
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

// ── ④ 凹多边形填充（demo ⑤ 镜像：五角星 / 闪电 / 箭头）──
// 十角交替半径轮廓 = 五角星；ear clipping 使强凹轮廓一次 fill 成型
function concavePath(g2d, kind) {
    g2d.beginPath();
    if (kind === 'star') {
        for (let i = 0; i < 10; ++i) {
            const a = -Math.PI / 2 + i * Math.PI / 5;
            const r = (i % 2 === 0) ? 44 : 18;
            const x = 66 + r * Math.cos(a), y = 70 + r * Math.sin(a);
            if (i === 0) g2d.moveTo(x, y); else g2d.lineTo(x, y);
        }
    } else if (kind === 'bolt') {
        const cx = 200, cy = 70;
        g2d.moveTo(cx - 4, cy - 34);  g2d.lineTo(cx + 14, cy - 34);
        g2d.lineTo(cx + 2, cy - 8);   g2d.lineTo(cx + 16, cy - 8);
        g2d.lineTo(cx - 10, cy + 34); g2d.lineTo(cx - 2, cy + 2);
        g2d.lineTo(cx - 16, cy + 2);
    } else {                                            // arrow
        const x = 270, m = 70, len = 105, h = 40, w = 10;
        g2d.moveTo(x, m - w / 2);         g2d.lineTo(x + len - h, m - w / 2);
        g2d.lineTo(x + len - h, m - h / 2); g2d.lineTo(x + len, m);
        g2d.lineTo(x + len - h, m + h / 2); g2d.lineTo(x + len - h, m + w / 2);
        g2d.lineTo(x, m + w / 2);
    }
    g2d.closePath();
}
const demo4 = () => {
    const c = G2D({ id: 'ds-g2d-concave', width: 400, height: 140 });
    c.setFillStyle('#F1C40F'); concavePath(c, 'star');  c.fill();   // 五角星
    c.setFillStyle('#F39C12'); concavePath(c, 'bolt');  c.fill();   // 闪电
    c.setFillStyle('#3498DB'); concavePath(c, 'arrow'); c.fill();   // 箭头
    return [
        c,
        Text({ text: t(L.hint4), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
    ];
};

// ── ⑤ 文本·渐变·变换（demo ⑥⑦⑧ 精选镜像）──
const demo5 = () => {
    const c = G2D({ id: 'ds-g2d-tgt', width: 400, height: 150 });
    // 文本：第 4 参字号缺省 14
    c.setFillStyle('#2C3E50'); c.fillText('fillText 文本绘制', 15, 12, 18);
    c.setFillStyle('#3498DB'); c.fillText('14px 缺省字号正文', 15, 42);
    // 渐变：水平线性（fillRect 走真渐变）
    c.setFillStyle(c.createLinearGradient(15, 0, 175, 0, '#3498DB', '#E74C3C'));
    c.fillRect(15, 70, 160, 36);
    // 径向（路径填充取起点色兜底 → 暖黄圆）
    c.setFillStyle(c.createRadialGradient(240, 88, 6, 240, 88, 32, '#FCE38A', '#F38181'));
    c.beginPath(); c.arc(240, 88, 32, 0, Math.PI * 2, false); c.fill();
    // 变换：save → translate 到轴心 → rotate → 画以原点为中心的矩形 → restore
    c.save();
    c.translate(340, 88); c.rotate(Math.PI / 6);
    c.setFillStyle('#27AE60'); c.fillRect(-38, -14, 76, 28);
    c.restore();
    return [
        c,
        Text({ text: t(L.hint5), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
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

const C4 = `// 十角交替半径轮廓 = 五角星；fill() ear clipping 凹多边形正确成型
function concavePath(c, kind) {
    c.beginPath();
    if (kind === 'star') {
        for (let i = 0; i < 10; ++i) {
            const a = -Math.PI / 2 + i * Math.PI / 5;
            const r = (i % 2 === 0) ? 44 : 18;
            const x = 66 + r * Math.cos(a), y = 70 + r * Math.sin(a);
            if (i === 0) c.moveTo(x, y); else c.lineTo(x, y);
        }
    } else if (kind === 'bolt') {
        // 闪电：强凹七点轮廓
        ...
    } else {
        // 箭头：矩形杆 + 三角头一次轮廓画完
        ...
    }
    c.closePath();
}

c.setFillStyle('#F1C40F'); concavePath(c, 'star');  c.fill();
c.setFillStyle('#F39C12'); concavePath(c, 'bolt');  c.fill();
c.setFillStyle('#3498DB'); concavePath(c, 'arrow'); c.fill();`;

const C5 = `// 文本：fillText(text, x, y, fontSize?)，第 4 参缺省 14
c.setFillStyle('#2C3E50'); c.fillText('fillText 文本绘制', 15, 12, 18);
c.setFillStyle('#3498DB'); c.fillText('14px 缺省字号正文', 15, 42);

// 渐变：对象传给 setFillStyle，fillRect 走真渐变
c.setFillStyle(c.createLinearGradient(15, 0, 175, 0, '#3498DB', '#E74C3C'));
c.fillRect(15, 70, 160, 36);

// 径向渐变（路径填充取起点色兜底 → 暖黄圆）
c.setFillStyle(c.createRadialGradient(240, 88, 6, 240, 88, 32, '#FCE38A', '#F38181'));
c.beginPath(); c.arc(240, 88, 32, 0, Math.PI * 2, false); c.fill();

// 变换：save → translate 轴心 → rotate → restore
c.save();
c.translate(340, 88); c.rotate(Math.PI / 6);
c.setFillStyle('#27AE60'); c.fillRect(-38, -14, 76, 28);
c.restore();`;

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    ...section(t(L.s3), demo3(), C3, innerW),
    ...section(t(L.s4), demo4(), C4, innerW),
    ...section(t(L.s5), demo5(), C5, innerW),
    propsTable(t, meta.props, innerW),
];
