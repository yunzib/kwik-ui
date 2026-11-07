// ============================================================================
// input_session.cppm — 输入会话契约件（键盘↔编辑器会话中介，批次 §中间层）
//
// 每棵 UI 树一份 TextInputSession（KwikRuntime 持有，挂根服务槽 kSvcInputSession）。
//
// 解耦定位（依赖单向性）：
//   - 编辑器侧（Input/TextArea/…）经 EventTarget::publishedTraits() 把"我是
//     什么输入器"发布上来（事件层 hook → KwikRuntime 装配 → publish）；
//   - 键盘侧（Keyboard）只订阅快照，不认识编辑器类型——新增编辑组件覆写
//     publishedTraits 即自动获得键盘支持，键盘零改动；
//   - 双方互不 import 对方，依赖箭头都指向本契约件。
//
// 本件零依赖（纯 std），可被 kwik.event / kwik.element / kwik.app 同向消费；
// RustRawEvent 之类的承载类型不在本件（注入归属见 View::kSvcEventRouter 槽）。
//
// v1 行为契约：发布/订阅即完毕，无任何策略内建（autoShow/autoFollow 等
// 自动化糖后续以显式开关叠加——快照只是数据源，默认行为零变化）。
// ============================================================================

export module kwik.core.input_session;

import std;

/**
 * @brief 编辑器输入模式（会话快照 mode 字段）
 *
 * 键盘键面选择的判据预留：number→数字键面。富文本等高阶编辑器对键盘
 * 而言仍是 Text/multiline——额外样式能力属组件私有面，不入会话契约。
 */
export enum class InputMode : std::uint8_t {
    Text,       // 通用文本（默认）
    Number      // 数字约束编辑（Input type:"number"）
};

/**
 * @brief 编辑器输入特征快照（会话中介数据载荷）
 *
 * 字段即编辑器公共约束的最小集：由 EventTarget::publishedTraits() 从
 * 编辑器 props（InputProps.isNumber/isPassword/readOnly 等）直映射。
 */
export struct TextInputTraits {
    InputMode mode = InputMode::Text;      // 输入模式（键面选择判据）
    bool password = false;                 // 密码态
    bool readOnly = false;                 // 只读（编辑器自拒；键盘仍可开）
    bool multiline = false;                // 多行（TextArea 等；Enter=换行语义）
    constexpr bool operator==(const TextInputTraits &) const = default;
};

/**
 * @brief 输入会话（每树一份）：焦点编辑器快照 + 订阅通道
 *
 * 发布点：FocusManager 焦点变化钩子（实例级，KwikRuntime init 装配）对
 * 当前聚焦目标取 publishedTraits()——聚焦编辑器发全量快照，聚焦非编辑器
 * /无焦点发 nullopt。订阅者（Keyboard）在快照变 nullopt 时收键盘。
 */
export class TextInputSession {
public:
    /// 快照形状：nullopt = 当前无聚焦编辑器（非编辑器/失焦均归一）
    using Snapshot = std::optional<TextInputTraits>;
    using Listener = std::function<void(const Snapshot &)>;

    /** @brief 发布快照（仅变更时通知；订阅者回调内退订自身安全——通知期拷贝） */
    void publish(const Snapshot &snap) {
        if (snapshot_ == snap) return;
        snapshot_ = snap;
        auto copy = listeners_;    // 回调重入（收键盘即退订）不破坏迭代器
        for (const auto &entry : copy) entry.second(snapshot_);
    }

    /** @brief 当前快照（不经订阅直接查询用） */
    const Snapshot &snapshot() const { return snapshot_; }

    /** @brief 订阅快照变更 @return 订阅令牌（removeListener 退订用） */
    std::size_t addListener(Listener fn) {
        const std::size_t id = nextId_++;
        listeners_.push_back({id, std::move(fn)});
        return id;
    }

    /** @brief 按令牌退订（未知令牌静默） */
    void removeListener(std::size_t id) {
        std::erase_if(listeners_, [id](const auto &entry) { return entry.first == id; });
    }

private:
    Snapshot snapshot_{};                                  ///< 最近发布快照（初始 nullopt）
    std::vector<std::pair<std::size_t, Listener>> listeners_;    ///< 订阅表（令牌→回调）
    std::size_t nextId_ = 1;                               ///< 令牌发号（0 保留=未订阅哨位）
};
