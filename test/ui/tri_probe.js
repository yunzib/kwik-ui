// ── 临时探针：叠渲 lazylist / stackindex / layer 三页 content()（验证后删除）──
import { Flex, Text, ScrollView } from 'kwikui';
import { content as lazyList } from '../docsite/js/pages/lazylist.js';
import { content as stackIndex } from '../docsite/js/pages/stackindex.js';
import { content as layerPage } from '../docsite/js/pages/layer.js';

const mark = (txt) => Text({ text: txt, fontSize: 16, fontWeight: 'bold', color: '#EF4444', margin: [20, 0, 8, 0] });

export default ScrollView({
    direction: 'vertical', width: 1082, background: '#F5F7FB',
    padding: [18, 12, 32, 28], showScrollbar: true,
}, [Flex({ direction: 'column' }, [
    mark('── lazylist ──'), ...lazyList(1030),
    mark('── stackindex ──'), ...stackIndex(1030),
    mark('── layer ──'), ...layerPage(1030),
])]);
