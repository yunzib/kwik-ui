module;

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

export module kwik.element.selection;

import kwik.core.types;
import kwik.element.view;

import std;

// ============================================================================
// selection.cppm — 文本选区（树级服务，槽 kSvcSelection）
//
// 三件套：
//   ① SelectionModel        纯模型：anchor/focus 字节偏移对，可单测
//   ② Selectable            组件能力接口：几何映射 + 取文 + 重绘（服务只
//                           认接口不出现具体组件类型——新组件实现同接口
//                           即并入全局选区，服务零改动）
//   ③ TextSelectionService  每树一份：全局唯一会话（互斥由单点状态保证）
//
// 事件层零改动依据：
//   - 拖选事件流：GestureRecognizer 对 PanMove/PointerMove/Up 均设
//     presetTarget=pressTarget → 拖出组件边界事件仍直投起始节点
//   - 滚动抑制：presetTarget 路径消费即止不冒泡 → 组件消费 PanMove 即
//     抑制祖先 ScrollView；滚轮链独立于目标消费，选区期间照常滚动
//   - 键盘路由：KeyAction 只投焦点 → 组件 acceptsFocus 后 Ctrl+C 天然到达
//
// 偏移空间约定：所有字节偏移均指「实际排版文本」（View 的 displayedText，
// maxLines 截断时含省略号）——用户看到什么就以什么为选择基准；cluster
// 值天然是合法 UTF-8 边界，另备 clampToUtf8Boundary 防御性钳制。
// ============================================================================

/**
 * @brief 选区纯模型 — anchor/focus 字节偏移对（无 UI 依赖，进 ctest）
 *
 * anchor = 拖选起点（不动端）；focus = 活动端（随拖动更新）。
 * 双向拖动均正确：range() 统一取 min/max 有序对。
 */
export struct SelectionModel {
	size_t anchor = 0;    ///< 拖选起点（字节偏移，UTF-8 边界）
	size_t focus = 0;     ///< 活动端（字节偏移，UTF-8 边界）

	/** @brief 选区是否为空（anchor == focus 折叠态） */
	bool empty() const { return anchor == focus; }

	/** @brief 有序字节区间 [min, max) */
	std::pair<size_t, size_t> range() const {
		return anchor < focus ? std::pair{anchor, focus} : std::pair{focus, anchor};
	}

	/** @brief 重置为折叠态（如新会话起点） */
	void reset(size_t off) { anchor = focus = off; }

	/** @brief 拖动扩展（只动 focus，anchor 不变） */
	void extendTo(size_t off) { focus = off; }
};

/**
 * @brief 可选文本组件能力接口 — 服务只认本接口，不出现具体组件类型
 *
 * 实现者：Text；TextView/Input/TextArea 等编辑组件实现同接口即可
 * 并入全局选区与复制链路。offsets 一律为 displayedText 字节空间。
 */
export class Selectable {
public:
	virtual ~Selectable() = default;

	/** @brief 点（全局逻辑坐标）→ 最近字形边界的字节偏移 */
	virtual size_t hitOffset(float globalX, float globalY) = 0;

	/** @brief 字节区间 → 选区高亮矩形（多行多段，全局坐标） */
	virtual std::vector<Rect> selectionRects(size_t startOff, size_t endOff) = 0;

	/** @brief 字节区间 → 文本内容（复制用，入参钳 UTF-8 边界） */
	virtual std::string selectedBytes(size_t startOff, size_t endOff) = 0;

	/** @brief 请求重绘（markDirty 封装——选区变化由服务回调） */
	virtual void requestRedraw() = 0;

	/** @brief 节点是否存活（TargetLiveness 查询——服务持裸指针防悬空） */
	virtual bool alive() const = 0;
};

/**
 * @brief 树级文本选区服务 — 全树同时最多一个选区（互斥单点保证）
 *
 * 生命周期：KwikRuntime 每树一份，经 setTreeService(kSvcSelection) 接线
 * （init/rebuild/错误页/HMR 四点 + teardown 清空，与 clipboard 同款）。
 * 跨重建安全：持 Selectable 裸指针，入口统一 pruneIfDead——reconcile
 * 复用节点（地址不变）选区自然存活；节点销毁则判死自动清，rebuildTree
 * 零挂钩。
 */
export class TextSelectionService {
public:
	/**
	 * @brief 开启新拖选会话（PointerDown）
	 *
	 * 全局互斥点：清旧会话（旧宿主重绘擦除高亮）后落新锚点。
	 * @param node      选区宿主（Selectable 实现者）
	 * @param anchorOff 起点字节偏移（hitOffset 产出）
	 */
	void begin(Selectable *node, size_t anchorOff) {
		pruneIfDead();
		if (node == host_) {
			model_.reset(anchorOff);    // 同节点重启（如再次按下）只换锚
		} else {
			clear();                    // 互斥：旧节点高亮随 clear 重绘擦除
			host_ = node;
			model_.reset(anchorOff);
		}
		dragging_ = true;
		node->requestRedraw();
	}

	/**
	 * @brief 拖动扩展（PanMove——presetTarget 直投，与 begin 同节点）
	 * @param node     宿主（与当前会话不符则忽略，防御性）
	 * @param focusOff 活动端字节偏移
	 */
	void extend(Selectable *node, size_t focusOff) {
		if (node != host_) return;
		model_.extendTo(focusOff);
		node->requestRedraw();
	}

	/** @brief 结束拖选（PointerUp/Cancel——选区驻留，仅退出拖动态） */
	void end() { dragging_ = false; }

	/** @brief 清空会话（FocusLost/文本变更；宿主在则重绘擦除高亮） */
	void clear() {
		if (host_) host_->requestRedraw();
		host_ = nullptr;
		model_ = {};
		dragging_ = false;
	}

	/** @brief node 是否为当前选区宿主（onDraw 高亮查询入口） */
	bool activeOn(const Selectable *node) const {
		const_cast<TextSelectionService *>(this)->pruneIfDead();
		return node != nullptr && node == host_;
	}

	/** @brief 取 node 的选区模型（非宿主返回 nullptr） */
	const SelectionModel *modelFor(const Selectable *node) const {
		if (!activeOn(node)) return nullptr;
		return &model_;
	}

private:
	/** 宿主判死即清（TargetLiveness——析构自动注销，见 event.cppm） */
	void pruneIfDead() {
		if (host_ && !host_->alive()) clear();
	}

	Selectable *host_ = nullptr;    // 选区宿主（裸指针，pruneIfDead 防悬空）
	SelectionModel model_;          // 当前选区（displayedText 字节空间）
	bool dragging_ = false;         // 拖选进行中（PointerUp 前）
};

/**
 * @brief 取本树文本选区服务（沿 parent 链上溯根节点槽 4；未接线返回 nullptr）
 * @param v 树内任意节点
 */
export inline TextSelectionService *selectionOf(const View &v) {
	return static_cast<TextSelectionService *>(v.treeService(View::kSvcSelection));
}

/**
 * @brief 防御性 UTF-8 边界钳制 — 偏移落在多字节序列中间时回退到首字节
 *
 * cluster 值天然是合法边界，本函数用于 selectAll 端点/外部传入等非
 * cluster 来源的兜底（中文 3 字节/emoji 4 字节切中间会产出非法串）。
 * @param text 目标文本（displayedText）
 * @param off  待钳制偏移（超界先收拢到 [0, size]）
 */
export inline size_t clampToUtf8Boundary(const std::string &text, size_t off) {
	if (text.empty()) return 0;
	if (off > text.size()) off = text.size();
	// 停在首字节（0xxx/11xx）上：连续字节 10xx 时回退
	while (off > 0 && (static_cast<unsigned char>(text[off]) & 0xC0) == 0x80) --off;
	return off;
}
