import { View, Text, Button, State, Flex, channel } from 'kwikui';

const state = new State({
    status: '就绪', statusLED: '#3fb950',
    result: '--', resultColor: '#8b949e',
    temp: 0, humidity: 0, sensorCount: 0,
    syncBusy: false, asyncBusy: false, coroBusy: false,
    logs: [],
});

function addLog(msg, clr) {
    const logs = state.logs.slice();
    logs.push({ msg: msg, clr: clr || '#8b949e' });
    if (logs.length > 30) logs.shift();
    state.logs = logs;
}

function sensorCard(val, unit, label, clr) {
    return View({
        flexGrow: 1, padding: 14,
        background: '#161b22', borderRadius: 6,
        borderWidth: 1, borderColor: '#30363d'
    }, [
        Text({ text: val + unit, fontSize: 28, fontWeight: 'bold', color: clr }),
        Text({ text: label, fontSize: 13, color: '#8b949e', margin: [6, 0, 0, 0] }),
    ]);
}

export default () => View({
    width: 800, height: 600,
    background: '#0d1117', padding: 12, gap: 8
}, [
    // ── 标题栏 ──
    View({ padding: 10, background: '#161b22', borderRadius: 6,
           borderWidth: 1, borderColor: '#30363d',
           flexDirection: 'row', justifyContent: 'spaceBetween',
           alignItems: 'center' }, [
        Text({ text: 'CHANNEL 通讯终端', fontSize: 18, fontWeight: 'bold',
               color: '#e6edf3' }),
        Flex({ direction: 'row', alignItems: 'center', gap: 8 }, [
            View({ width: 10, height: 10, background: '#3fb950', borderRadius: 5 }),
            Text({ text: '系统正常', fontSize: 13, color: '#3fb950', fontWeight: 'bold' }),
        ]),
    ]),

    // ── 状态栏 ──
    View({
        padding: 10, background: '#161b22', borderRadius: 6,
        borderWidth: 1, borderColor: '#30363d',
        flexDirection: 'row', justifyContent: 'spaceBetween'
    }, [
        Flex({ direction: 'row', alignItems: 'center', gap: 8 }, [
            View({ width: 10, height: 10, background: state.statusLED, borderRadius: 5 }),
            Text({ text: state.status, fontSize: 14, fontWeight: 'bold', color: state.statusLED }),
        ]),
        Text({ text: '结果: ' + state.result, fontSize: 14, color: state.resultColor }),
    ]),

    // ── 传感器 ──
    Flex({ direction: 'row', gap: 8 }, [
        sensorCard(state.temp.toFixed(1), ' C', '温度', '#58a6ff'),
        sensorCard(state.humidity.toFixed(1), '%', '湿度', '#3fb950'),
    ]),

    // ── 操作按钮 2×2 ──
    Flex({ direction: 'row', gap: 8 }, [
        View({ width: 382, padding: 12, background: '#161b22',
               borderRadius: 6, borderWidth: 1, borderColor: '#30363d', gap: 8 }, [
            Text({ text: '① 通知 C++', fontSize: 15, fontWeight: 'bold', color: '#58a6ff' }),
            Flex({ direction: 'row', justifyContent: 'spaceBetween', alignItems: 'center' }, [
                Text({ text: 'JS 发送 -> C++ 打印', fontSize: 13, color: '#8b949e' }),
                Button({ text: '发送通知', width: 100, height: 32,
                    background: '#58a6ff', textColor: '#ffffff',
                    borderRadius: 4, fontSize: 13,
                    onClick: function() {
                        channel.send('button_click', { id: Date.now() });
                        state.status = '通知已发'; state.statusLED = '#3fb950';
                        state.result = 'button_click'; state.resultColor = '#3fb950';
                        addLog('-> 发送通知 button_click', '#58a6ff');
                    }
                }),
            ]),
        ]),
        View({ width: 382, padding: 12, background: '#161b22',
               borderRadius: 6, borderWidth: 1, borderColor: '#30363d', gap: 8 }, [
            Text({ text: '② 同步调用', fontSize: 15, fontWeight: 'bold', color: '#d29922' }),
            Flex({ direction: 'row', justifyContent: 'spaceBetween', alignItems: 'center' }, [
                Text({ text: 'JS call -> 立即返回', fontSize: 13, color: '#8b949e' }),
                Button({ text: state.syncBusy ? '执行中' : '获取配置',
                    width: 100, height: 32,
                    background: state.syncBusy ? '#21262d' : '#d29922',
                    textColor: '#ffffff', borderRadius: 4, fontSize: 13,
                    onClick: async function() {
                        state.syncBusy = true;
                        state.status = '同步请求...'; state.statusLED = '#d29922';
                        addLog('-> 调用 get_config', '#d29922');
                        var r = await channel.call('get_config', { key: 'theme' });
                        state.result = r; state.resultColor = '#d29922';
                        state.status = '同步完成'; state.statusLED = '#3fb950';
                        addLog('<- 返回: ' + r, '#3fb950'); state.syncBusy = false;
                    }
                }),
            ]),
        ]),
    ]),
    Flex({ direction: 'row', gap: 8 }, [
        View({ width: 382, padding: 12, background: '#161b22',
               borderRadius: 6, borderWidth: 1, borderColor: '#30363d', gap: 8 }, [
            Text({ text: '③ 异步线程', fontSize: 15, fontWeight: 'bold', color: '#f0883e' }),
            Flex({ direction: 'row', justifyContent: 'spaceBetween', alignItems: 'center' }, [
                Text({ text: '子线程 500ms -> 主线程', fontSize: 13, color: '#8b949e' }),
                Button({ text: state.asyncBusy ? '下载中' : '下载文件',
                    width: 100, height: 32,
                    background: state.asyncBusy ? '#21262d' : '#f0883e',
                    textColor: '#ffffff', borderRadius: 4, fontSize: 13,
                    onClick: async function() {
                        state.asyncBusy = true;
                        state.status = '线程下载...'; state.statusLED = '#f0883e';
                        addLog('-> 调用 start_download', '#f0883e');
                        var r = await channel.call('start_download', { url: 'file' });
                        state.result = r; state.resultColor = '#f0883e';
                        state.status = '线程完成'; state.statusLED = '#3fb950';
                        addLog('<- 返回: ' + r, '#3fb950'); state.asyncBusy = false;
                    }
                }),
            ]),
        ]),
        View({ width: 382, padding: 12, background: '#161b22',
               borderRadius: 6, borderWidth: 1, borderColor: '#30363d', gap: 8 }, [
            Text({ text: '④ 异步协程', fontSize: 15, fontWeight: 'bold', color: '#7ee787' }),
            Flex({ direction: 'row', justifyContent: 'spaceBetween', alignItems: 'center' }, [
                Text({ text: 'co_await 线程池 -> 主线程', fontSize: 13, color: '#8b949e' }),
                Button({ text: state.coroBusy ? '处理中' : '处理文件',
                    width: 100, height: 32,
                    background: state.coroBusy ? '#21262d' : '#7ee787',
                    textColor: '#ffffff', borderRadius: 4, fontSize: 13,
                    onClick: async function() {
                        state.coroBusy = true;
                        state.status = '协程处理...'; state.statusLED = '#7ee787';
                        addLog('-> 调用 process_file', '#7ee787');
                        var r = await channel.call('process_file', { path: '/data' });
                        state.result = r; state.resultColor = '#7ee787';
                        state.status = '协程完成'; state.statusLED = '#3fb950';
                        addLog('<- 返回: ' + r, '#3fb950'); state.coroBusy = false;
                    }
                }),
            ]),
        ]),
    ]),

    // ── 日志 ──
    View({
        padding: 10, background: '#161b22', borderRadius: 6,
        borderWidth: 1, borderColor: '#30363d',
        flexGrow: 1, gap: 8
    }, [
        Text({ text: '操作日志', fontSize: 14, fontWeight: 'bold', color: '#8b949e' }),
        View({ padding: 8, background: '#0d1117', borderRadius: 4,
               borderWidth: 1, borderColor: '#21262d', flexGrow: 1
        }, state.logs.length > 0
            ? state.logs.map(function(item, i) { return Text({
                text: '[' + (i < 9 ? '0' : '') + (i + 1) + '] ' + item.msg,
                fontSize: 13, color: item.clr, margin: [0, 0, 3, 0]
              }); })
            : [Text({ text: '[--] 等待操作...', fontSize: 13, color: '#484f58' })]
        ),
    ]),
]);

// ── C++ 事件 ──
channel.on('sensor:temp', function(d) {
    var p = d.split(',');
    state.temp = parseFloat(p[0].split(':')[1]) || 0;
    state.humidity = parseFloat(p[1].split(':')[1]) || 0;
    state.sensorCount = parseInt(p[2].split(':')[1]) || 0;
    state.status = '传感器 #' + state.sensorCount; state.statusLED = '#58a6ff';
    state.result = state.temp.toFixed(1) + 'C  ' + state.humidity.toFixed(1) + '%';
    state.resultColor = '#58a6ff';
    addLog('传感器 #' + state.sensorCount + ': ' + state.temp.toFixed(1) + 'C ' + state.humidity.toFixed(1) + '%', '#58a6ff');
});

channel.on('show_toast', function(d) {
    state.status = d; state.statusLED = '#f85149';
    addLog('C++ 通知: ' + d, '#f85149');
});