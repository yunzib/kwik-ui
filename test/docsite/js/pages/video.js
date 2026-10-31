// ── Video 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/extensions/video.md。
// 单演示小节镜像 test/ui/video.js 验证 demo（Null 后端静态帧验证管线）+
// 方法卡（play/pause/seek 按 id 定位）。demo 侧改动必须同步本页（reverse 也是）。
import { Text, Video } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'video',
    group: 'adv',
    title: { zh: 'Video 视频', en: 'Video' },
    desc: {
        zh: '插件式视频组件（扩展）：解码后端抽象（FFmpeg / mpv / 平台原生可插拔），'
            + '拉取模型渲染——绘制线程取最新帧上传纹理，解码线程不触碰 Vulkan；'
            + 'src/autoplay/loop/muted 声明式配置 + play/pause/seek 命令式方法。',
        en: 'Pluggable video widget (extension): decoder backends are swappable '
            + '(FFmpeg / mpv / platform), rendered pull-style — the draw thread uploads '
            + 'the latest frame, decoders never touch Vulkan; declarative src/autoplay/ '
            + 'loop/muted plus imperative play/pause/seek.',
    },
    props: [
        { title: { zh: '播放配置', en: 'Playback' }, rows: [
            ['src', 'string', '-', { zh: '视频源路径（相对运行目录）', en: 'Source path (relative to run dir)' }],
            ['autoplay', 'boolean', 'false', { zh: '创建即播放', en: 'Start on creation' }],
            ['loop', 'boolean', 'false', { zh: '循环播放', en: 'Loop' }],
            ['muted', 'boolean', 'false', { zh: '静音', en: 'Muted' }],
        ] },
        { title: { zh: '方法与机制', en: 'Methods & Model' }, rows: [
            ['play() / pause() / seek(t)', '-', '-', { zh: '命令式控制（seek 跳到第 t 秒）', en: 'Imperative control (seek to second t)' }],
            ['方法定位', '-', '-', { zh: '按 id 从树中 findById 定位 C++ Video；未写 id 工厂自动生成占位 id', en: 'Methods locate the C++ Video by id; the factory generates a placeholder id when omitted' }],
            ['属性绑定', '-', '-', { zh: 'src 等支持 State 绑定（创建层 applyBindings + reconcile 增量）', en: 'src etc. accept State binding (applyBindings + incremental reconcile)' }],
        ] },
    ],
};

const L = {
    s1: { zh: '播放演示（autoplay）', en: 'Playback demo (autoplay)' },
    hint1: { zh: '当前随库实现为 Null 后端：静态帧验证「解码 → 纹理 → 绘制」全管线'
               + '（demo.mp4 占位源）；接入真实解码器只需实现 VideoBackend 接口经工厂注入，'
               + '组件层零改动。v1 复用通用指针事件，onEnd / onTimeUpdate 待后端加槽位后扩展。',
            en: 'The in-tree backend is Null: a static frame proving the decode → texture → '
                + 'draw pipeline with demo.mp4 as placeholder. Real decoders implement the '
                + 'VideoBackend interface and register a factory — the widget stays unchanged. '
                + 'v1 reuses generic pointer events; onEnd / onTimeUpdate come with backend slots.' },
};

// ── ① 播放演示（demo 镜像：autoplay 静态帧）──
const demo1 = () => [
    Video({ src: 'demo.mp4', width: 480, height: 270, autoplay: true }),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
];

// 代码卡（与上方演示逐行同源 + 方法面；改动演示同步这里）
const C1 = `import { Video } from 'kwikui';

// 声明式：src/autoplay/loop/muted（均支持 State 绑定）
Video({ src: 'demo.mp4', width: 480, height: 270, autoplay: true });

// 命令式：工厂返回带方法的句柄——按 id 从树中定位 C++ Video 对象
//（未显式给 id 时工厂自动生成占位 id，直接调方法即可）
const v = Video({ id: 'player', src: 'demo.mp4', width: 480, height: 270 });
v.play();
v.pause();
v.seek(12.5);    // 跳到第 12.5 秒`;

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
