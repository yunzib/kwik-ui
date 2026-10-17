// ============================================================================
// 模块: kwik.element.clipboard_service
// 剪贴板服务抽象 — 元素层不依赖平台模块，经树服务槽取本树 ClipboardService*
//
// 接线：KwikRuntime 每树一份适配器，init/重根/HMR 三处
// setTreeService(View::kSvcClipboard, ...)（teardown 清空，与其他树级
// 服务槽同语义，见 KwikRuntime::teardownJsBoundRuntime）；
// 未接线（nullptr）或平台无实现时组件层安全跳过，无副作用。
// ============================================================================
module;
#include <string>

export module kwik.element.clipboard_service;

import std;

export class ClipboardService {
public:
    virtual ~ClipboardService() = default;

    /** @brief 读取系统剪贴板 UTF-8 文本
     *  @param out 成功时写入；剪贴板为空（不含文本数据）返回 false
     *  @return false = 未提供剪贴板能力/打开失败/无文本数据 */
    virtual bool getText(std::string &out) = 0;

    /** @brief 写入系统剪贴板 UTF-8 文本
     *  @param text UTF-8 内容（空文本按无意义写入处理）
     *  @return false = 未提供剪贴板能力/打开失败 */
    virtual bool setText(const std::string &text) = 0;
};
