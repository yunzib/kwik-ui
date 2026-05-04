import { View, Text, Button, Column, Row, State, Channel } from 'ui';

// 1. 简洁的状态定义
const appState = new State({
  count: 0,
  name: "User",
  items: []
});

// 2. 组件定义 - 类QML语法
const App = () => View(
  // 属性设置
  { width: 800, height: 600, background: "#f5f5f5" },
  
  // 子组件（自动识别数组中的组件）
  [
    Column(
      { spacing: 20, padding: 30, align: "center" },
      [
        // 文本组件 - 响应式绑定
        Text({
          text: () => `Hello, ${appState.name}!`, // 函数响应式
          fontSize: 32,
          color: "#333"
        }),
        
        // 计数器显示
        Text({
          text: () => `Count: ${appState.count}`,
          fontSize: 24,
          color: "#666"
        }),
        
        // 按钮行
        Row(
          { spacing: 10 },
          [
            Button({
              text: "Increase",
              width: 120,
              height: 50,
              background: "#4CAF50",
              onClick: async () => {
                // 直接修改状态，UI自动更新
                appState.count++;
                
                // 使用Channel发送事件
                await uiEvents.send({
                  type: "count_changed",
                  value: appState.count
                });
              }
            }),
            
            Button({
              text: "Reset",
              width: 120,
              height: 50,
              background: "#f44336",
              onClick: () => {
                appState.count = 0;
              }
            }),
            
            Button({
              text: "Add Item",
              width: 120,
              height: 50,
              background: "#2196F3",
              onClick: async () => {
                const item = {
                  id: Date.now(),
                  name: `Item ${appState.items.length + 1}`
                };
                
                // 批量更新状态
                appState.update({
                  items: [...appState.items, item]
                });
              }
            })
          ]
        ),
        
        // 动态列表 - 自动响应数组变化
        Column(
          { spacing: 8, visible: () => appState.items.length > 0 },
          () => appState.items.map(item =>
            View(
              {
                width: 300,
                height: 40,
                background: "#fff",
                borderRadius: 8,
                shadow: "0 2px 8px rgba(0,0,0,0.1)"
              },
              [
                Row(
                  { align: "center", padding: 10 },
                  [
                    Text({
                      text: item.name,
                      fontSize: 16,
                      flex: 1
                    }),
                    
                    Button({
                      text: "Remove",
                      width: 80,
                      height: 30,
                      background: "#ff9800",
                      onClick: () => {
                        appState.items = appState.items.filter(i => i.id !== item.id);
                      }
                    })
                  ]
                )
              ]
            )
          )
        )
      ]
    )
  ]
);

// 3. 创建通信通道
const uiEvents = new Channel();
const dataChannel = new Channel();

// 4. 后台服务协程
async function backgroundService() {
  console.log("Background service started");
  
  while (true) {
    // 等待UI事件
    const event = await uiEvents.receive();
    
    switch (event.type) {
      case "count_changed":
        console.log(`Count changed to: ${event.value}`);
        
        // 发送数据到UI
        await dataChannel.send({
          type: "log",
          message: `Count updated to ${event.value}`
        });
        break;
        
      case "item_added":
        // 处理项目添加
        break;
    }
  }
}

// 5. UI事件处理器协程
async function uiEventHandler() {
  while (true) {
    const data = await dataChannel.receive();
    
    // 处理来自后台的数据
    switch (data.type) {
      case "log":
        console.log("From service:", data.message);
        break;
    }
  }
}

// 6. 应用启动
const app = App();

// 启动协程
backgroundService();
uiEventHandler();

// 导出应用实例
export default app;