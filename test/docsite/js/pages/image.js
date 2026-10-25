// ── Image 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/image.md。
// 两个示例小节：缩放策略 fit / SVG 矢量图。
import { View, Flex, Text, Image } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'image',
    group: 'base',
    title: { zh: 'Image 图片', en: 'Image' },
    desc: {
        zh: '支持 PNG / JPEG / BMP / GIF / SVG 的 GPU 纹理图片组件，'
            + '带圆角裁剪与 contain / cover / fill / none 四种缩放策略。',
        en: 'GPU-textured image component for PNG / JPEG / BMP / GIF / SVG, '
            + 'with rounded clipping and contain / cover / fill / none fitting.',
    },
    props: [
        { title: { zh: '数据源', en: 'Source' }, rows: [
            ['src', 'string', "''", { zh: '图片路径（PNG/JPEG/BMP/GIF/SVG）', en: 'Image path' }],
            ['data', 'ArrayBuffer', '-', { zh: '内存像素数据（与 src 二选一）', en: 'In-memory pixels (alternative to src)' }],
            ['width / height', 'number', '-', { zh: 'data 模式必需；同时作为显示尺寸', en: 'Required for data mode; also display size' }],
        ] },
        { title: { zh: '显示', en: 'Display' }, rows: [
            ['fit', 'contain | cover | fill | none', 'cover', { zh: '缩放策略', en: 'Scaling strategy' }],
            ['opacity', 'number (0..1)', '1', { zh: '透明度', en: 'Opacity' }],
            ['borderRadius', 'number', '0', { zh: '圆角裁剪', en: 'Rounded clipping' }],
        ] },
    ],
};

const L = {
    s1: { zh: '缩放策略 fit', en: 'Fit modes' },
    s2: { zh: 'SVG 矢量图', en: 'SVG asset' },
};

const C1 = `import { Image } from 'kwikui';

Image({ src: 'photo.png', width: 180, height: 120, fit: 'cover' });
Image({ src: 'icon.png', width: 180, height: 120, fit: 'contain' });`;
const demo1 = () => [
    Flex({ direction: 'row', gap: 12 }, [
        View({ width: 180, height: 120, background: '#F1F5F9', borderRadius: 8 }, [
            Image({ src: '../../test/image/Web Analytics.png', width: 180, height: 120, fit: 'cover', borderRadius: 8 }),
        ]),
        View({ width: 180, height: 120, background: '#F1F5F9', borderRadius: 8 }, [
            Image({ src: '../../test/image/Web Application.png', width: 180, height: 120, fit: 'contain' }),
        ]),
    ]),
    Text({ text: '左：cover 铺满裁切；右：contain 完整显示', fontSize: 11, color: '#94A3B8', margin: [2, 0, 0, 0] }),
];

const C2 = `Image({ src: 'home.svg', width: 40, height: 40, fit: 'contain' });
Image({ src: 'search.svg', width: 40, height: 40, fit: 'contain' });`;
const demo2 = () => [
    Flex({ direction: 'row', gap: 18, alignItems: 'center' }, [
        Image({ src: '../../test/image/home.svg', width: 40, height: 40, fit: 'contain' }),
        Image({ src: '../../test/image/search.svg', width: 40, height: 40, fit: 'contain' }),
        Image({ src: '../../test/image/system.svg', width: 40, height: 40, fit: 'contain' }),
        Image({ src: '../../test/image/menu.svg', width: 40, height: 40, fit: 'contain' }),
    ]),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    propsTable(t, meta.props, innerW),
];
