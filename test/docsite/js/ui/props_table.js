// ── PropsTable —— 属性表 ──
// 四列（属性 / 类型 / 默认值 / 说明），表头深底白字、数据行斑马纹；
// 支持分组小节标题行（跨全表宽浅底粗体，属性按语义分块）。
// 行数据契约：[name, type, def, { zh, en }]，说明列经 t() 取当前语言。
// 入参契约（向后兼容）：平数组 = 无分组直渲染（旧页面同构）；
// 分组数组 [{ title: {zh,en}, rows: [...] }, ...] = 每组前插小节标题行。
// 列宽：类型列 170 容纳联合类型串（'number | [h,v] | [t,r,b,l]' 单行放得下），
// 全列 wordWrap 兜底——超长内容在列宽内换行，不溢出列界。
import { View, Flex, Text } from 'kwikui';

const COLS = [150, 170, 90];    // 前三列固定（属性/类型/默认值）；末列说明列吃剩余
// 末列宽 = 表体宽 (w-2) − 前三列合计
const LAST_W = (w) => w - 2 - COLS[0] - COLS[1] - COLS[2];

// 单元格：head=true 表头白字，k=0 属性名蓝字；wordWrap 长文本列内换行
const cell = (c, k, head, w) => Text({
    text: (c === '' || c == null) ? '-' : String(c),
    fontSize: 12,
    color: head ? '#E2E8F0' : (k === 0 ? '#2563EB' : '#334155'),
    fontWeight: head ? 'bold' : 'normal',
    width: k < COLS.length ? COLS[k] : LAST_W(w),
    padding: [6, 0, 0, 10],
    wordWrap: true,
});

// 数据/表头行。行容器必须 Flex——View 是纵向单列流且不解析 direction
// （Flex 专属 prop），用 View 四格会竖排叠放
const row = (cells, i, head, w) => Flex({
    direction: 'row', width: w - 2,    // 撑满表体（w - 左右边框）
    background: head ? '#1E293B' : (i % 2 === 1 ? '#F8FAFC' : '#FFFFFF'),
}, cells.map((c, k) => cell(c, k, head, w)));

// 小节标题行：跨全表宽浅底粗体（属性语义分块的节名）
const sectionRow = (title, w) => Flex({
    direction: 'row', width: w - 2, background: '#F1F5F9',
}, [Text({
    text: title, fontSize: 12, fontWeight: 'bold', color: '#475569',
    width: w - 12, padding: [7, 0, 0, 10],
})]);

/**
 * 属性表（antd API 表等价物）
 * @param t             语言帮手（由调用方传入，表头/说明列取当前语言）
 * @param groupsOrRows  分组结构 [{title:{zh,en}, rows:[[name,type,def,{zh,en}],...]}]
 *                      或旧平数组 [[name,type,def,{zh,en}],...]（无分组直渲染）
 * @param w             表格外宽（内容区净宽，main 下传；默认值保持独立可运行）
 */
export const propsTable = (t, groupsOrRows, w = 884) => {
    // 分组判定：首元素含 rows 字段即分组结构（空表两态都渲染空体）
    const isGrouped = groupsOrRows.length > 0 && groupsOrRows[0] && !!groupsOrRows[0].rows;
    const body = isGrouped
        ? groupsOrRows.flatMap(g => [
              sectionRow(t(g.title), w),
              ...g.rows.map((r, i) => row([r[0], r[1], r[2], t(r[3])], i, false, w)),
          ])
        : groupsOrRows.map((r, i) => row([r[0], r[1], r[2], t(r[3])], i, false, w));
    return View({
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
        ...body,
    ]);
};
