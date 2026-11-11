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

// ============================================================================
// 富文本 run 序列序列化（剪贴板自定义格式 kwik/text-runs-v2 的字节编解码）
//
// 布局（小端原生写读，读端逐字段边界校验）：
//   magic "KWTVR2"(6B) + u32 runCount
//   + 每 run{ u32 recordLen + record{ u32 textLen + text + f32 fontSize +
//     u8 weight + u8 italic + 4B color(RGBA) + u8 underline +
//     u8 strikethrough + u8 hasLink + [u32 linkLen + link（hasLink≠0 时）] } }
//
// recordLen = record 本体字节数（不含 recordLen 自身）。读端按 recordLen
// 消费——record 内未知尾部字段自动跳过（后续版本加字段无需改读端）。
// ============================================================================

/** @brief run 序列剪贴板格式注册名（平台 RegisterClipboardFormat 用） */
export inline constexpr const char *kTextRunsClipboardFormat = "kwik/text-runs-v2";

/** @brief 序列化 run 序列（样式 + link 全保留）为剪贴板自定义格式字节串 */
export std::string serializeTextRuns(const std::vector<TextRun> &runs);

/** @brief 反序列化剪贴板自定义格式字节串为 run 序列
 *  @return false = magic 不符/截断/字段越界（畸形数据；调用方回落纯文本粘贴） */
export bool deserializeTextRuns(const std::string &bytes, std::vector<TextRun> &out);
