// ── G3D 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/extensions/g3d.md。
// 两节结构镜像 test/ui/g3d.js 验证 demo（glTF 模型加载 / 内建基本体组合）。
// demo 侧改动必须同步本页（reverse 也是）。G3D 走 eager 实例（工厂即建
// C++ 对象），命令在树建立前调用；整树 rebuild 重放同套命令——幂等重建。
import { Text, G3D } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'g3d',
    group: 'adv',
    title: { zh: 'G3D 3D 绘制', en: 'G3D 3D' },
    desc: {
        zh: '3D 场景绘制（扩展组件）：glTF 模型加载 + 内建基本体（立方体 / 球 / 地面）+ '
            + '坐标轴，轨道相机 Pan 拖拽旋转与 autoRotate；渲染走独立 Mesh 深度管线。',
        en: '3D scene drawing (extension): glTF loading plus built-in primitives '
            + '(box / sphere / plane) and axes, with an orbit camera — drag to rotate, '
            + 'autoRotate by default; rendered on a dedicated depth-tested mesh pipeline.',
    },
    props: [
        { title: { zh: '画布与行为', en: 'Canvas & Behavior' }, rows: [
            ['width / height', 'number', '300 × 150', { zh: '逻辑宽 / 高（3D 内容在元素矩形视口内渲染）', en: 'Logical size (3D renders in the element viewport)' }],
            ['background', '#rrggbb', '透明', { zh: '画布背景色', en: 'Canvas background' }],
            ['autoRotate', 'boolean', 'true', { zh: '自动旋转（每帧 yaw + 0.02 rad；拖拽期间暂停）', en: 'Auto rotate (yaw +0.02 rad/frame; paused while dragging)' }],
        ] },
        { title: { zh: '常用方法', en: 'Methods' }, rows: [
            ['loadModel(path)', 'string', '-', { zh: '加载 glTF（.glb/.gltf，相对运行目录；不可达仅记日志）', en: 'Load glTF (relative to run dir; logs if missing)' }],
            ['addBox / addSphere / addPlane', 'number…', '-', { zh: '内建基本体（尺寸 + 世界平移 tx,ty,tz）', en: 'Primitives (size + world translation)' }],
            ['rotateTo(yaw, pitch) / setColor(c)', '-', '-', { zh: '绝对轨道角（弧度，pitch 限 ±1.5）/ 统一基础色', en: 'Absolute orbit angles (rad) / base color' }],
            ['clear() / showAxes(bool)', '-', '-', { zh: '清空网格 / 坐标轴开关（X红 Y绿 Z蓝，默认显示）', en: 'Clear meshes / toggle axes (default on)' }],
        ] },
    ],
};

const L = {
    s1: { zh: 'glTF 模型加载（cube.glb + 自动旋转）', en: 'glTF loading (cube.glb + autoRotate)' },
    s2: { zh: '内建基本体组合（地面 + 立方体 + 球 + 坐标轴）', en: 'Primitives (plane + box + sphere + axes)' },
    hint1: { zh: 'loadModel 路径相对运行目录（与 image 的 src 约定一致）；setColor 统一基础色，'
               + '光照为对象空间方向光（兰伯特漫反射 + 0.25 环境光）。按住拖拽可转视角——'
               + '水平改 yaw、垂直改 pitch（±86°），拖拽期间自动旋转暂停。',
            en: 'loadModel paths are relative to the run directory (same as image src); setColor '
                + 'sets the base color with object-space lambert lighting. Drag to orbit — yaw '
                + 'horizontally, pitch vertically (±86°); autoRotate pauses while dragging.' },
    hint2: { zh: '不加载模型也能搭场景：addPlane 地面（法线 +Y）、addBox 立方体（逐面法线）、'
               + 'addSphere UV 球（24×12）；tx,ty,tz 世界平移分离摆放。rotateTo 设绝对轨道角、'
               + 'showAxes 显隐坐标轴（红 X / 绿 Y / 蓝 Z）。',
            en: 'Scenes without models: addPlane ground (+Y normal), addBox per-face normals, '
                + 'addSphere UV sphere (24×12); tx,ty,tz place them apart. rotateTo sets absolute '
                + 'orbit angles; showAxes toggles the RGB axes.' },
};

// ── ① glTF 模型加载（demo 首节镜像）──
const demo1 = () => {
    const m = G3D({ id: 'ds-g3d-model', width: 420, height: 300, background: '#1a1a2e' });
    m.loadModel('../../test/cube.glb');
    m.setColor('#4fc3f7');
    m.autoRotate(true);
    return [
        m,
        Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
    ];
};

// ── ② 内建基本体组合（demo 次节镜像）──
const demo2 = () => {
    const p = G3D({ id: 'ds-g3d-prims', width: 420, height: 300, background: '#1a1a2e' });
    p.setColor('#ffb74d');
    p.addPlane(3.0, 3.0);                 // 地面（原点）
    p.addBox(1.4, -1.5, 0, 0);            // 立方体（左侧）
    p.addSphere(0.5, 24, 12, 1.5, 0, 0);  // 球（右侧）
    p.rotateTo(0.5, 0.4);                 // 绝对轨道角（弧度）
    p.autoRotate(false);
    p.showAxes(true);
    return [
        p,
        Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
    ];
};

// 代码卡（与上方演示逐行同源；改动演示同步这里）
const C1 = `import { G3D } from 'kwikui';

const m = G3D({ id: 'ds-g3d-model', width: 420, height: 300, background: '#1a1a2e' });
m.loadModel('../../test/cube.glb');   // 路径相对运行目录，.glb/.gltf
m.setColor('#4fc3f7');                // 统一基础色
m.autoRotate(true);                   // 自动旋转（拖拽期间暂停）`;

const C2 = `const p = G3D({ id: 'ds-g3d-prims', width: 420, height: 300, background: '#1a1a2e' });

p.setColor('#ffb74d');
p.addPlane(3.0, 3.0);                 // 地面（法线 +Y，原点）
p.addBox(1.4, -1.5, 0, 0);            // 立方体（size + 世界平移 tx,ty,tz）
p.addSphere(0.5, 24, 12, 1.5, 0, 0);  // UV 球（radius, slices, stacks + 平移）
p.rotateTo(0.5, 0.4);                 // 绝对轨道角（弧度，pitch 限 ±1.5）
p.autoRotate(false);
p.showAxes(true);                     // 坐标轴（红 X / 绿 Y / 蓝 Z）`;

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    propsTable(t, meta.props, innerW),
];
