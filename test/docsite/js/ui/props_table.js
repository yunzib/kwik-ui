// ── PropsTable —— 属性表 ──
// 四列（属性 / 类型 / 默认值 / 说明），表头深底白字、数据行斑马纹。
// 行数据契约：[name, type, def, { zh, en }]，说明列经 t() 取当前语言。
import { View, Flex, Text } from 'kwikui';

const COLS = [150, 90, 120];    // 前三列固定；末列说明列吃剩余（净宽 − 前三列 − 2 边框）

// 单行：head=true 表头样式（深底白字），否则斑马纹数据行。
// 行容器必须 Flex——View 是纵向单列流且不解析 direction（Flex 专属 prop），
// 用 View 四格会竖排叠放
const row = (cells, i, head, w) => Flex({
    direction: 'row', width: w - 2,    // 撑满表体（w - 左右边框）
    background: head ? '#1E293B' : (i % 2 === 1 ? '#F8FAFC' : '#FFFFFF'),
}, cells.map((c, k) => Text({
    text: (c === '' || c == null) ? '-' : String(c),
    fontSize: 12,
    color: head ? '#E2E8F0' : (k === 0 ? '#2563EB' : '#334155'),
    fontWeight: head ? 'bold' : 'normal',
    width: k < COLS.length ? COLS[k] : w - 362,    // 末列 = (w - 2) − 前三列 360，随净宽伸缩
    padding: [6, 0, 0, 10],
})));

/**
 * 属性表（antd API 表等价物）
 * @param t    语言帮手（由调用方传入，表头/说明列取当前语言）
 * @param rows 行数据 [[name, type, def, {zh,en}], ...]
 * @param w    表格外宽（内容区净宽，main 下传；默认值保持独立可运行）
 */
export const propsTable = (t, rows, w = 884) => View({
    width: w, margin: [0, 0, 24, 4],
    background: '#FFFFFF', borderColor: '#E2E8F0',
    borderWidth: 1, borderRadius: 8,
}, [
    row([
        t({ zh: '属性', en: 'Property' }),
        t({ zh: '类型', en: 'Type' }),
        t({ zh: '默认值', en: 'Default' }),
        t({ zh: '说明', en: 'Description' }),
    ], 0, true, w),
    ...rows.map((r, i) => row([r[0], r[1], r[2], t(r[3])], i, false, w)),
]);
