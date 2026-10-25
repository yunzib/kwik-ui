// ── Channel 文档页 ──
// 章节编号与 docs/4. Channel.md 对齐（4.1 通信模式 … 4.9 线程安全），示例全部摘自
// 可运行 demo：./example.exe channel（JS：test/ui/channel.js，C++：test/example.cpp）。
// 事实口径按源码，不按手稿：
//   · channel.send → jsSend：同步直投 C++ onHandlers_，无队列、不等帧
//   · Channel::send → 入队 → 主线程每帧 flush 派发，同 topic 帧内合并取最后一条
//   · call / Channel::handle 三档：同步 return Data / 线程 + respond / 协程 CoroTask
//   · channel.off 按 JS 值指针身份摘首个命中；channel.handle 无 off
// 站点无 C++ 端：4.2/4.3 只验证载荷与注册注销，4.4 靠 JS 侧 handle 完成真实往返，
// C++ 代码块给真实源码，不伪造回包。
import { View, Flex, Text, Button, State, channel } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { demoCard, codeCard, codePair, chip } from '../ui/page_kit.js';

const JS = (sub, code) => ({ tag: 'JS 端', sub, accent: 'js', code });
const CPP = (sub, code) => ({ tag: 'C++ 端', sub, accent: 'cpp', code });

export const meta = {
    id: 'channel',
    group: 'guide',
    title: { zh: 'Channel 双向通信', en: 'Channel' },
    desc: {
        zh: 'C++ 与 JS 之间的双向通道：单向通知（send / on）与请求-响应（call / handle）。'
            + '所有示例摘自 ./example.exe channel（test/ui/channel.js + test/example.cpp）。',
        en: 'The two-way path between C++ and JS: notifications (send / on) and request-reply '
            + '(call / handle). All snippets come from ./example.exe channel '
            + '(test/ui/channel.js + test/example.cpp).',
    },
    props: [
        { title: { zh: 'JS 端（kwikui.channel）', en: 'JS side (kwikui.channel)' }, rows: [
            ['channel.send(topic, data)', 'void', '-', { zh: '发通知，无返回值；同步直投 C++ handler', en: 'Fire a notification; dispatched synchronously' }],
            ['channel.on(topic, fn)', 'void', '-', { zh: '收通知；按函数身份去重（同一引用只留一份）', en: 'Receive; deduped by function identity' }],
            ['channel.off(topic, fn)', 'void', '-', { zh: '注销；第二参须为注册时的同一引用', en: 'Unregister; second arg must be the same reference' }],
            ['channel.call(topic, data)', 'Promise<any>', '-', { zh: '请求-响应；三档 handler 全空则 reject', en: 'Request-reply; rejects when no handler answers' }],
            ['channel.handle(topic, fn)', 'void', '-', { zh: 'JS 侧应答者；无 off，与 C++ 侧抢占同一表', en: 'JS-side answerer; no off, shares lookup with C++' }],
        ] },
        { title: { zh: 'C++ 端（kwik.engine.channel）', en: 'C++ side (kwik.engine.channel)' }, rows: [
            ['Channel::send(topic, data)', 'void', '-', { zh: '推给 JS；任意线程安全，入队后每帧 flush 派发', en: 'Push to JS; thread-safe, flushed per frame' }],
            ['Channel::on(topic, fn)', 'void', '-', { zh: '接 JS 的 send（主线程注册）', en: 'Receive JS channel.send' }],
            ['Channel::handle(topic, fn → Data)', 'void', '-', { zh: '同步应答：返回值即结果', en: 'Sync answer: the returned Data is the result' }],
            ['Channel::handle(topic, (d, respond))', 'void', '-', { zh: '异步应答：respond 须回主线程调用', en: 'Async answer: respond on the main thread' }],
            ['Channel::handle(topic, fn → CoroTask)', 'void', '-', { zh: '协程应答：co_return 传结果，final_suspend 自动 respond', en: 'Coroutine answer: co_return; auto-respond on final_suspend' }],
            ['Channel::getMainThreadQueue()', 'TaskQueue&', '-', { zh: '从任意线程投递任务回主线程（respond / 续体）', en: 'Post a task back to the main thread' }],
            ['Channel::thread_pool() / main_thread()', 'awaitable', '-', { zh: '协程内切线程：worker / 主线程', en: 'Switch threads inside a coroutine' }],
        ] },
        { title: { zh: '边界与陷阱', en: 'Boundaries & traps' }, rows: [
            ['注册写进 content()', '每轮重建 +1 份', '-', { zh: '新闭包身份不同 → 一次推送回调 N 次；handle 无 off，多余永久泄漏', en: 'New closures defeat dedupe → N calls per push; extra handles leak' }],
            ['off 传新闭包', '静默无操作', '-', { zh: '函数对象不同，指针比对落空——不报错也不摘除', en: 'Different function object → pointer miss, nothing removed' }],
            ['off(topic) 少传第二参', 'TypeError', '-', { zh: '没有「清空 topic」的重载；一次只摘首个命中项', en: 'No "clear topic" overload; removes the first hit only' }],
            ['Data::asString()', 'std::get', '-', { zh: '非字符串抛 std::bad_variant_access，不是返回空串', en: 'Throws on non-strings; does not return ""' }],
            ['对象 / 数组载荷', '→ JSON 字符串', '-', { zh: '跨边界即 stringify，与真字符串不可区分', en: 'Stringified at the boundary, indistinguishable from a string' }],
            ['数字载荷', '→ double', '-', { zh: 'isNumber() 真、isInt() 假；int64 只来自 C++ 构造', en: 'isNumber() true, isInt() false; int64 only from C++' }],
        ] },
    ],
};

const L = {
    demo: { zh: '⓪ 示例：./example.exe channel', en: '⓪ Demo: ./example.exe channel' },
    demoLead: {
        zh: 'JS 端 test/ui/channel.js，C++ 端 test/example.cpp 的 ChannelTest::setup()。'
            + '启动后 C++ 每 3 秒推一组温湿度，共 6 组，推完再推一条 show_toast：',
        en: 'UI in test/ui/channel.js, C++ host in ChannelTest::setup() of test/example.cpp. On start, '
            + 'C++ pushes temp/humidity every 3s for 6 rounds, then one show_toast:',
    },
    dcUi: { zh: '界面', en: 'UI' },
    dcApi: { zh: '走的 API', en: 'API used' },
    s41: { zh: '4.1 通信模式', en: '4.1 Communication modes' },
    dDir: { zh: '方向', en: 'Direction' },
    dMode: { zh: '模式', en: 'Mode' },
    dJs: { zh: 'JS API', en: 'JS API' },
    dCpp: { zh: 'C++ API', en: 'C++ API' },

    s42: { zh: '4.2 JS → C++ 通知', en: '4.2 JS → C++ notification' },
    f42a: { zh: '同步直投：jsSend 在 send 的调用栈里当场执行 C++ handler，无队列、不等帧。', en: 'Synchronous: jsSend runs the C++ handler inside the send call stack — no queue, no next frame.' },
    f42b: { zh: '对象 / 数组跨边界已 stringify，C++ 收到的是 {"id":…}。', en: 'Objects are stringified at the boundary: C++ receives {"id":…}.' },

    s43: { zh: '4.3 C++ → JS 通知', en: '4.3 C++ → JS notification' },
    f43a: { zh: 'C++ 侧入队，主线程每帧 Channel::flush 派发；同一 topic 一帧内多次发送只留最后一条。', en: 'Queued by C++ and dispatched in Channel::flush; same-topic sends in one frame collapse to the last.' },
    f43b: { zh: '注销必须交回注册时的那个函数本身：注册表按 JS 值指针比对。', en: 'Unregister with the very function you registered: the registry compares JS value pointers.' },
    reg: { zh: '注册', en: 'Register' },
    unreg: { zh: '注销', en: 'Unregister' },
    badOff: { zh: 'off 传新闭包', en: 'off with a new closure' },
    badOffs: { zh: '指针未中', en: 'pointer misses' },
    silent: { zh: '静默无操作', en: 'silent no-op' },
    registered: { zh: '接收中', en: 'receiving' },
    unregistered: { zh: '未接收', en: 'idle' },

    s44: { zh: '4.4 双向调用', en: '4.4 Request-reply' },
    f44a: { zh: 'JS 侧永远只有一行 await；差别全在 C++ 怎么回包，三选一。', en: 'The JS side is always one await; the difference is how C++ answers.' },
    c44b: { zh: 'B · 线程 + respond：先返回，干完再回主线程回包', en: 'B · Thread + respond: answer later from the main thread' },
    c44c: { zh: 'C · 协程：同一件事写成一行到底', en: 'C · Coroutine: one straight-line flow' },
    callDemo: { zh: '本站无 C++，往返由 JS 侧应答者完成，Promise 与 reject 都是真的：', en: 'No C++ here: the round-trip is served by a JS answerer — Promise and reject are real:' },
    ok: { zh: '有应答者：await 拿到结果', en: 'Answerer present: await resolves' },
    none: { zh: '无应答者：reject', en: 'No answerer: rejects' },
    resLine: { zh: '结果', en: 'Result' },
    errLine: { zh: '错误', en: 'Error' },
    coro1: { zh: 'Data& 参数在首次 co_await 后成悬挂引用——挂起前先拷：`Channel::Data copy = d;`', en: 'Data& dangles after the first co_await — copy it before suspending.' },
    coro2: { zh: '推荐签名改传值 `(Data d)`，协程帧拥有副本。', en: 'Prefer by-value `(Data d)` so the frame owns its copy.' },
    coro3: { zh: 'co_await thread_pool() 切 worker，co_await main_thread() 切回；co_return 的结果在 final_suspend 里自动 respond 并销毁帧。', en: 'co_await thread_pool() → worker, co_await main_thread() → main; co_return responds in final_suspend and destroys the frame.' },

    s46: { zh: '4.6 数据类型：Channel::Data', en: '4.6 Payload types: Channel::Data' },
    f46a: { zh: '对象 / 数组跨边界即 JSON 串，C++ 侧收到后自行解析。', en: 'Objects and arrays arrive as JSON strings; parse them yourself.' },
    f46b: { zh: 'asString() 对非字符串抛 std::bad_variant_access，取值前先 isXxx() 判断。', en: 'asString() throws std::bad_variant_access on non-strings — check isXxx() first.' },
    pSent: { zh: 'JS 传入', en: 'JS sends' },
    pLand: { zh: 'C++ 收到', en: 'C++ receives' },
    pRead: { zh: '读取', en: 'Read with' },

    s49: { zh: '4.9 线程安全', en: '4.9 Thread safety' },
    tOp: { zh: '操作', en: 'Operation' },
    tSafe: { zh: '线程', en: 'Thread' },
    tNote: { zh: '说明', en: 'Note' },

    sRef: { zh: '速查：API 与边界', en: 'Reference: APIs and edges' },
};

// ── 表格积木：列宽按可用宽等比 ──
const cellText = (txt, k, head, wid) => Text({
    text: txt, fontSize: 12,
    color: head ? '#E2E8F0' : (k === 0 ? '#334155' : '#2563EB'),
    fontWeight: head ? 'bold' : 'normal',
    width: wid, padding: [6, 0, 0, 10], wordWrap: true,
});
const tableRow = (cells, i, head, w, cols) => Flex({
    direction: 'row', width: w - 2,
    background: head ? '#1E293B' : (i % 2 === 1 ? '#F8FAFC' : '#FFFFFF'),
}, cells.map((c, k) => cellText(c, k, head, cols[k] === undefined ? cols[cols.length - 1] : cols[k])));
const tableFrame = (w, rows) => View({
    width: w, margin: [0, 0, 14, 4], background: '#FFFFFF',
    borderColor: '#E2E8F0', borderWidth: 1, borderRadius: 8,
}, rows);
const grid = (w, rat) => {
    const g = 6 * (rat.length - 1);
    return rat.map((r) => Math.floor((w - g) * r));
};

// ⓪ 示例：界面上的动作 ↔ 走的 API
const DEMO_ROWS = [
    [{ zh: '① 通知 C++', en: '① Notify C++' }, "channel.send('button_click', …) → Channel::on"],
    [{ zh: '② 同步调用', en: '② Sync call' }, "await channel.call('get_config', …) → Channel::handle"],
    [{ zh: '③ 异步线程', en: '③ Threaded' }, "await channel.call('start_download', …) → handle + respond"],
    [{ zh: '④ 异步协程', en: '④ Coroutine' }, "await channel.call('process_file', …) → CoroTask"],
    [{ zh: '传感器卡（自动）', en: 'Sensors (auto)' }, "channel.on('sensor:temp') ← Channel::send × 6"],
    [{ zh: '状态栏（自动）', en: 'Status bar (auto)' }, "channel.on('show_toast') ← Channel::send × 1"],
];
const demoTable = (w) => {
    const cols = grid(w, [0.26, 0.74]);
    return tableFrame(w, [
        tableRow([t(L.dcUi), t(L.dcApi)], 0, true, w, cols),
        ...DEMO_ROWS.map((r, i) => tableRow([t(r[0]), r[1]], i, false, w, cols)),
    ]);
};

// 4.1 通信模式
const MODE_ROWS = [
    ['JS → C++', { zh: '通知（单向）', en: 'notify (one-way)' }, 'channel.send(topic, data)', 'Channel::on(topic, handler)'],
    ['C++ → JS', { zh: '通知（单向）', en: 'notify (one-way)' }, 'channel.on(topic, handler)', 'Channel::send(topic, data)'],
    ['JS → C++', { zh: '调用（双向）', en: 'call (round-trip)' }, 'channel.call(topic, data)', 'Channel::handle(topic, handler)'],
];
const modeTable = (w) => {
    const cols = grid(w, [0.16, 0.18, 0.31, 0.35]);
    return tableFrame(w, [
        tableRow([t(L.dDir), t(L.dMode), t(L.dJs), t(L.dCpp)], 0, true, w, cols),
        ...MODE_ROWS.map((r, i) => tableRow([r[0], t(r[1]), r[2], r[3]], i, false, w, cols)),
    ]);
};

// 4.6 载荷落地
const PAYLOAD_ROWS = [
    ["'kwik'", 'string', 'd.asString()'],
    ['42', 'double', { zh: 'd.asNumber()（isInt() 为假）', en: 'd.asNumber() (isInt() is false)' }],
    ['true', 'bool', 'd.asBool()'],
    ['{a:1} / [1,2]', 'string（JSON）', { zh: 'd.asString() 后 JSON.parse', en: 'd.asString() then JSON.parse' }],
    ['null / 未传', 'monostate', 'd.isNull()'],
    [{ zh: '（C++ 侧构造）', en: '(built in C++)' }, 'int64', 'd.asInt()'],
];
const payloadTable = (w) => {
    const cols = grid(w, [0.22, 0.24, 0.54]);
    return tableFrame(w, [
        tableRow([t(L.pSent), t(L.pLand), t(L.pRead)], 0, true, w, cols),
        ...PAYLOAD_ROWS.map((r, i) => tableRow([r[0], r[1], t(r[2])], i, false, w, cols)),
    ]);
};

// 4.9 线程安全
const THREAD_ROWS = [
    ['Channel::send()', { zh: '任意线程', en: 'any' }, { zh: '入队，主线程 flush 时派发', en: 'Queued, flushed per frame' }],
    ['Channel::on() / handle()', { zh: '主线程', en: 'main' }, { zh: '注册 handler', en: 'Handler registration' }],
    ['Channel::getMainThreadQueue()', { zh: '任意线程', en: 'any' }, { zh: '投递回主线程（respond / 续体）', en: 'Post back for respond' }],
    ['JS send / on / off / call', { zh: '主线程', en: 'main' }, { zh: 'JS 引擎单线程', en: 'Single-threaded JS engine' }],
    ['协程 thread_pool / main_thread', { zh: '任意线程', en: 'any' }, { zh: '基于 Scheduler 线程池', en: 'Scheduler thread pool' }],
    ['JS handler 执行', { zh: '主线程', en: 'main' }, { zh: 'flush 中 JS_Call', en: 'JS_Call inside flush' }],
];
const threadTable = (w) => {
    const cols = grid(w, [0.34, 0.16, 0.50]);
    return tableFrame(w, [
        tableRow([t(L.tOp), t(L.tSafe), t(L.tNote)], 0, true, w, cols),
        ...THREAD_ROWS.map((r, i) => tableRow([r[0], t(r[1]), t(r[2])], i, false, w, cols)),
    ]);
};

// 4.4 B 的时序条
const stepRow = (n, side, api, desc, w) => Flex({
    direction: 'row', alignItems: 'start', gap: 8, margin: [3, 0, 3, 0],
}, [
    Text({ text: n, fontSize: 12, color: '#2563EB', width: 20 }),
    Text({
        text: side, fontSize: 10.5, fontWeight: 'bold',
        color: side === 'JS' ? '#B45309' : '#0369A1', width: 34, padding: [1, 0, 0, 0],
    }),
    Text({ text: api, fontSize: 11.5, color: '#0F172A', width: 236, wordWrap: true }),
    Text({ text: desc, fontSize: 11.5, color: '#64748B', width: Math.max(110, w - 366), wordWrap: true }),
]);
const ASYNC_STEPS = [
    { n: '①', side: 'JS', api: "await channel.call('start_download')", d: { zh: 'Promise 挂起，UI 照常跑', en: 'Pending Promise; UI keeps running' } },
    { n: '②', side: 'C++', api: 'handle(d, respond) 命中', d: { zh: '主线程执行后立即返回', en: 'Runs on main, returns at once' } },
    { n: '③', side: 'C++', api: 'std::thread', d: { zh: 'worker 干活（IO / 网络）', en: 'Worker does the slow work' } },
    { n: '④', side: 'C++', api: 'getMainThreadQueue().post', d: { zh: '回主线程后 respond(Data)', en: 'respond(Data) on main thread' } },
    { n: '⑤', side: 'JS', api: 'await 之后继续', d: { zh: 'Promise 恢复，结果在下一行', en: 'Promise resumes with the result' } },
];
const stepsPanel = (innerW) => View({
    width: innerW - 40, padding: 10, margin: [4, 0, 0, 0], background: '#F8FAFC',
    borderColor: '#E2E8F0', borderWidth: 1, borderRadius: 8,
}, ASYNC_STEPS.map((s) => stepRow(s.n, s.side, s.api, t(s.d), innerW - 40)));

// ════════════════════════════════════════════════════════════════════════════
// 状态与演示
// ════════════════════════════════════════════════════════════════════════════
const chSt = new State({
    registered: false, hits: 0, badOff: 0,     // 4.3 注册 / 注销
    topic: '', res: '', err: '',               // 4.4 往返
});

// 4.3：on / off 必须同一引用
const subscribe = (topic, fn) => {
    channel.on(topic, fn);
    return () => channel.off(topic, fn);
};
const onSensor = () => { chSt.hits = chSt.hits + 1; };
let stopSensor = null;
const doRegister = () => {
    if (stopSensor) return;                    // 幂等
    stopSensor = subscribe('docsite:sensor', onSensor);
    chSt.registered = true;
};
const doUnregister = () => {
    if (stopSensor) { stopSensor(); stopSensor = null; }
    chSt.registered = false;
};
const doBadOff = () => {
    channel.off('docsite:sensor', () => { chSt.hits = chSt.hits + 1; });
    chSt.badOff = chSt.badOff + 1;
};

// 4.4：真实往返（无 C++ → 用 JS 侧应答者）
channel.handle('docsite:sum', (payload) => {
    const { a, b } = JSON.parse(payload);      // 对象到这一侧已是 JSON 串
    return a + b;
});
const runCall = (topic, payload) => {
    chSt.topic = topic + '(' + (typeof payload === 'string' ? "'" + payload + "'" : JSON.stringify(payload)) + ')';
    chSt.res = '…';
    chSt.err = '';
    channel.call(topic, payload)
        .then((r) => { chSt.res = String(r); })
        .catch((e) => { chSt.res = '—'; chSt.err = String(e); });
};

// ════════════════════════════════════════════════════════════════════════════
// 代码块：全部摘自 test/ui/channel.js / test/example.cpp
// ════════════════════════════════════════════════════════════════════════════
const C2js = `// test/ui/channel.js —— 卡片①
channel.send('button_click', { id: Date.now() });`;

const C2cpp = `// test/example.cpp
Channel::on("button_click",
  [](const Channel::Data &d) {
    Log::info("[通知] 'button_click': {}",
              d.asString());   // {"id":1761…}
  });`;

const C3cpp = `// test/example.cpp —— 每 3 秒推一组，共 6 组
std::string payload = "temp:25.3,humidity:68.5,"
                      "count:1,status:normal";
Channel::send("sensor:temp", payload);
Channel::setTimeout(3000,
  [count] { sensorLoop(count + 1); });`;

const C3js = `// test/ui/channel.js —— 收到字符串，自己拆
channel.on('sensor:temp', function(d) {
  var p = d.split(',');
  state.temp = parseFloat(p[0].split(':')[1]) || 0;
  state.humidity = parseFloat(p[1].split(':')[1]) || 0;
});`;

const C3off = `// ✓ 同一引用：命中并摘掉
const onTemp = (d) => { state.temp = Number(d); };
channel.on('sensor:temp', onTemp);
channel.off('sensor:temp', onTemp);

// ✗ 新闭包：指针不同，静默不摘除
channel.off('sensor:temp', (d) => { state.temp = Number(d); });`;

const C4js = `// test/ui/channel.js —— 卡片②：等 C++ 回话
var r = await channel.call('get_config',
                           { key: 'theme' });
// r === "dark_theme"`;

const C4a = `// test/example.cpp —— A · 同步返回
Channel::handle("get_config",
  [](const Channel::Data &d) -> Channel::Data {
    return Channel::Data("dark_theme");
  });`;

const C4b = `// test/example.cpp —— B · 线程 + respond
Channel::handle("start_download",
  [](const Channel::Data &d, auto respond) {
    std::thread([d, respond] {
      std::this_thread::sleep_for(
        std::chrono::milliseconds(500));
      Channel::getMainThreadQueue().post(
        [respond, s = std::string(d.asString())] {
          respond(Channel::Data("Downloaded: " + s));
        });
    }).detach();
  });`;

const C4c = `// test/example.cpp —— C · 协程
Channel::handle("process_file",
  [](const Channel::Data &d) -> Channel::CoroTask {
    Channel::Data copy = d;    // 挂起前先拷
    co_await Channel::thread_pool();   // → worker
    std::this_thread::sleep_for(
      std::chrono::milliseconds(100));
    auto s = "Processed: " + std::string(copy.asString());
    co_await Channel::main_thread();   // → 主线程
    co_return Channel::Data(s);        // 自动 respond
  });`;

// ════════════════════════════════════════════════════════════════════════════
// 装配
// ════════════════════════════════════════════════════════════════════════════
const h = (txt) => Text({
    text: txt, fontSize: 15, fontWeight: 'bold', color: '#0F172A', margin: [22, 0, 8, 6],
});
const line = (txt, color, innerW, top) => Text({
    text: txt, fontSize: 12, color: color || '#475569',
    width: innerW - 32, wordWrap: true, margin: [top === undefined ? 4 : top, 0, 0, 0],
});
const bullet = (txt, innerW) => Text({
    text: '· ' + txt, fontSize: 12, color: '#334155',
    width: innerW - 32, wordWrap: true, margin: [3, 0, 0, 0],
});
const codeLabel = (txt, innerW) => Text({
    text: txt, fontSize: 12.5, fontWeight: 'bold', color: '#0F172A',
    width: innerW - 32, wordWrap: true, margin: [12, 0, 6, 2],
});

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),

    // ⓪ 示例
    h(t(L.demo)),
    demoCard([
        line('./example.exe channel', '#1D4ED8', innerW, 0),
        line(t(L.demoLead), '#475569', innerW, 4),
        demoTable(innerW - 32),
    ], innerW),

    // 4.1
    h(t(L.s41)),
    modeTable(innerW - 32),

    // 4.2
    h(t(L.s42)),
    codePair(JS('channel.send', C2js), CPP('Channel::on', C2cpp), innerW),
    bullet(t(L.f42a), innerW),
    bullet(t(L.f42b), innerW),

    // 4.3
    h(t(L.s43)),
    codePair(CPP('Channel::send', C3cpp), JS('channel.on', C3js), innerW),
    bullet(t(L.f43a), innerW),
    demoCard([
        Flex({ direction: 'row', alignItems: 'center', flexWrap: 'wrap', gap: 10 }, [
            Button({ text: t(L.reg), width: 90, height: 30, fontSize: 12, background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6, onClick: doRegister }),
            Button({ text: t(L.unreg), width: 90, height: 30, fontSize: 12, background: '#475569', color: '#FFFFFF', borderRadius: 6, onClick: doUnregister }),
            chip(
                chSt.registered ? t(L.registered) : t(L.unregistered),
                chSt.registered ? '#DCFCE7' : '#F1F5F9',
                chSt.registered ? '#166534' : '#64748B',
            ),
        ]),
        Flex({ direction: 'row', alignItems: 'center', flexWrap: 'wrap', gap: 10, margin: [10, 0, 0, 0] }, [
            Button({ text: t(L.badOff), width: 120, height: 30, fontSize: 12, background: '#B91C1C', color: '#FFFFFF', borderRadius: 6, onClick: doBadOff }),
            Text({ text: t(L.badOffs) + ': ' + chSt.badOff, fontSize: 12, color: '#B91C1C' }),
            ...(chSt.badOff > 0 ? [chip(t(L.silent), '#FEF2F2', '#B91C1C')] : []),
        ]),
        line(t(L.f43b), '#475569', innerW, 10),
    ], innerW),
    codeCard(C3off, innerW),

    // 4.4
    h(t(L.s44)),
    line(t(L.f44a), '#475569', innerW, 0),
    codePair(JS('channel.call', C4js), CPP('Channel::handle（同步）', C4a), innerW),
    demoCard([
        line(t(L.callDemo), '#475569', innerW, 0),
        Flex({ direction: 'row', alignItems: 'center', flexWrap: 'wrap', gap: 10, margin: [10, 0, 0, 0] }, [
            Button({
                text: t(L.ok), width: 150, height: 30, fontSize: 12,
                background: '#0F766E', color: '#FFFFFF', borderRadius: 6,
                onClick: () => runCall('docsite:sum', { a: 3, b: 4 }),
            }),
            Button({
                text: t(L.none), width: 140, height: 30, fontSize: 12,
                background: '#B91C1C', color: '#FFFFFF', borderRadius: 6,
                onClick: () => runCall('docsite:none', { a: 1 }),
            }),
        ]),
        line(t(L.resLine) + ': ' + (chSt.topic ? chSt.topic + ' → ' + (chSt.res || '—') : '—'), '#0F172A', innerW, 10),
        ...(chSt.err ? [line(t(L.errLine) + ': ' + chSt.err, '#B91C1C', innerW, 3)] : []),
    ], innerW),
    codeLabel(t(L.c44b), innerW),
    stepsPanel(innerW),
    codeCard(C4b, innerW),
    codeLabel(t(L.c44c), innerW),
    codeCard(C4c, innerW),
    bullet(t(L.coro1), innerW),
    bullet(t(L.coro2), innerW),
    bullet(t(L.coro3), innerW),

    // 4.6
    h(t(L.s46)),
    payloadTable(innerW - 32),
    bullet(t(L.f46a), innerW),
    bullet(t(L.f46b), innerW),

    // 4.9
    h(t(L.s49)),
    threadTable(innerW - 32),

    // 速查
    h(t(L.sRef)),
    propsTable(t, meta.props, innerW),
];
