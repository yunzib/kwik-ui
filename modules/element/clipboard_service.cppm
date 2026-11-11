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
#include <vector>

export module kwik.element.clipboard_service;

import std;
import kwik.core.props;

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

    /** @brief 读取富文本 run 序列（应用注册格式；外部应用来源/平台不支持返回 false）
     *
     * 默认实现返回 false——Input/TextArea 等纯文本组件不受影响。
     * 实现方负责反序列化校验（畸形数据返回 false，组件层回落纯文本粘贴）。 */
    virtual bool getRuns(std::vector<TextRun> &out) { return false; }

    /** @brief 写入富文本 run 序列（追加自定义格式，须在 setText 之后调用以共存）
     *  @param plainFallback 同内容的纯文本（同时写入 CF_UNICODETEXT 供外部应用）
     *  @return false = 平台不支持/写入失败（调用方回落 setText） */
    virtual bool setRuns(const std::vector<TextRun> &runs, const std::string &plainFallback) { return false; }
};
