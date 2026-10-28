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
    s3: { zh: '透明度 opacity', en: 'Opacity' },
    s4: { zh: '圆角裁剪 borderRadius', en: 'Rounded clipping' },
};

const C1 = `import { Image } from 'kwikui';

Image({ src: 'photo.png', width: 180, height: 120, fit: 'cover' });
Image({ src: 'icon.png', width: 180, height: 120, fit: 'contain' });`;
const demo1 = () => [
    Flex({ direction: 'row', gap: 12 }, [
        View({ width: 180, height: 120, background: '#F1F5F9', borderRadius: 8 }, [
            Image({ src: '../../test/image/p1.png', width: 180, height: 120, fit: 'cover', borderRadius: 8 }),
        ]),
        View({ width: 180, height: 120, background: '#F1F5F9', borderRadius: 8 }, [
            Image({ src: '../../test/image/p2.png', width: 180, height: 120, fit: 'contain' }),
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

const C3 = `import { Image, View, Flex } from 'kwikui';

// opacity: 0..1 递减——深色底衬上淡出层次清晰（白底图片淡出宜垫深衬）
Flex({ direction: 'row', gap: 16 }, [
    View({ width: 150, height: 100, background: '#334155' }, [
        Image({ src: 'app.png', width: 150, height: 100, fit: 'cover', opacity: 1 }),
    ]),
    View({ width: 150, height: 100, background: '#334155' }, [
        Image({ src: 'app.png', width: 150, height: 100, fit: 'cover', opacity: 0.7 }),
    ]),
    View({ width: 150, height: 100, background: '#334155' }, [
        Image({ src: 'app.png', width: 150, height: 100, fit: 'cover', opacity: 0.4 }),
    ]),
    View({ width: 150, height: 100, background: '#334155' }, [
        Image({ src: 'app.png', width: 150, height: 100, fit: 'cover', opacity: 0.15 }),
    ]),
]);`;
const demo3 = () => [
    Flex({ direction: 'row', gap: 16 }, [
        View({ width: 150, height: 100, background: '#334155' }, [
            Image({ src: '../../test/image/p1.png', width: 150, height: 100, fit: 'cover', opacity: 1 }),
        ]),
        View({ width: 150, height: 100, background: '#334155' }, [
            Image({ src: '../../test/image/p1.png', width: 150, height: 100, fit: 'cover', opacity: 0.7 }),
        ]),
        View({ width: 150, height: 100, background: '#334155' }, [
            Image({ src: '../../test/image/p1.png', width: 150, height: 100, fit: 'cover', opacity: 0.4 }),
        ]),
        View({ width: 150, height: 100, background: '#334155' }, [
            Image({ src: '../../test/image/p1.png', width: 150, height: 100, fit: 'cover', opacity: 0.15 }),
        ]),
    ]),
    Text({ text: 'opacity 1 → 0.7 → 0.4 → 0.15（深色底衬）：逐级淡出露出底衬，白底图片建议垫深衬演示', fontSize: 11, color: '#94A3B8', margin: [2, 0, 0, 0] }),
];

const C4 = `import { Image, Flex } from 'kwikui';

// borderRadius: 像素按圆角裁剪图片四角（0=直角，超大值=胶囊两端半圆）
Flex({ direction: 'row', gap: 16, alignItems: 'center' }, [
    Image({ src: 'photo.png', width: 150, height: 100, fit: 'cover', borderRadius: 0 }),
    Image({ src: 'photo.png', width: 150, height: 100, fit: 'cover', borderRadius: 8 }),
    Image({ src: 'photo.png', width: 150, height: 100, fit: 'cover', borderRadius: 24 }),
    Image({ src: 'photo.png', width: 150, height: 100, fit: 'cover', borderRadius: 999 }),
]);`;
const demo4 = () => [
    Flex({ direction: 'row', gap: 16, alignItems: 'center' }, [
        Image({ src: '../../test/image/p2.png', width: 150, height: 100, fit: 'cover', borderRadius: 0 }),
        Image({ src: '../../test/image/p2.png', width: 150, height: 100, fit: 'cover', borderRadius: 8 }),
        Image({ src: '../../test/image/p2.png', width: 150, height: 100, fit: 'cover', borderRadius: 24 }),
        Image({ src: '../../test/image/p2.png', width: 150, height: 100, fit: 'cover', borderRadius: 999 }),
    ]),
    Text({ text: 'borderRadius 0 → 8 → 24 → 999：直角 → 圆角 → 大圆角 → 胶囊（裁剪作用于图片自身像素）', fontSize: 11, color: '#94A3B8', margin: [2, 0, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    ...section(t(L.s3), demo3(), C3, innerW),
    ...section(t(L.s4), demo4(), C4, innerW),
    propsTable(t, meta.props, innerW),
];
