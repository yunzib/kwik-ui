// ============================================================================
// dropdown.cpp — Dropdown 下拉选择控件
//
// 视觉: 触发区 (背景+文字+箭头) + 展开时菜单覆盖层
// 交互: 点击触发区切换展开; 点击菜单项选中+关闭; 点击外部关闭
// 文字: 通过 TextRenderPipeline 排版渲染, 元素持有 shared_ptr
//
// 菜单 Layer 化（M2 架构）：
//   菜单不再画在 Dropdown::onDraw 内（inline 方式存在 z 遮挡、跨父级溢出、
//   兄弟重绘擦菜单、hitTest 冲突、点外部无法关闭等问题），而是作为独立浮层
//   节点 MenuView 注册进 LayerStack：
//     - open()  → registerLayerView + drawnElsewhere_=true（base 树跳过绘制/命中）
//     - close() → unregisterLayerView + drawnElsewhere_=false + base 全树重录（防 ghost）
//     - 始终作为 Dropdown 子节点挂载：parent 链保证内部 markDirty 冒泡到 base 根
//       唤醒主循环（application.cpp renderFrame 只查 base 树 dirty）
//     - hitTest 全屏返回 this：LayerStack hitTest 层优先 → 菜单开启期间点击全部
//       被吞下并关闭（原生 select 行为）
//   模块接口不暴露 MenuView（"模块内禁止前置声明"约束），Dropdown 持 View*
//   成员 menuLayer_，经公共接口 dropdownProps()/menuRect()/commitSelection() 解耦。
// ============================================================================
module;
#include <cstring>
#include <string>
#include <vector>

module kwik.element.dropdown;
import kwik.element.view;
import kwik.element.layer_view; // LayerStack（菜单层注册）
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.render.command;
import kwik.element.typed_prop;
import kwik.event;

import std;

// ════════════════════════════════════════════════════════
// MenuView — 下拉菜单浮层层节点（本 TU 内部实现，不导出）
// ════════════════════════════════════════════════════════
namespace {

/**
 * @brief 菜单浮层层节点
 *
 * 与 Dropdown 经公共接口解耦（owner_.dropdownProps()/owner_.menuRect()/
 * owner_.commitSelection()/owner_.setOpen()），不依赖 Dropdown 私有成员。
 */
class MenuView : public View {
public:
    explicit MenuView(Dropdown &owner) : owner_(owner) {
        // 挂为 Dropdown 子节点：仅取 parent 链用于脏标记冒泡，
        // 绘制/命中由 LayerStack 直接接管（drawnElsewhere_）。
    }

    ~MenuView() override {
        // HMR / 树重建兜底：确保不残留悬空层指针
        if (registered_) LayerStack::instance().unregisterLayerView(this);
    }

    /** @brief 展开菜单：定位到触发区正下方并注册进 LayerStack */
    void open() {
        if (registered_) return;
        frame = owner_.menuRect();    // 全局坐标定位（触发区正下方）
        scrollOffset_ = 0;
        hoveredIndex_ = -1;
        drawnElsewhere_ = true;    // base 树跳过本节点绘制与命中
        LayerStack::instance().registerLayerView(this);
        registered_ = true;
        markAllDirty();    // 层首帧全量重录（脏标记向上冒泡唤醒主循环）
    }

    /** @brief 收起菜单：注销 + 复原 base 覆盖区（防 ghost 残留） */
    void close() {
        if (!registered_) return;
        LayerStack::instance().unregisterLayerView(this);
        registered_ = false;
        drawnElsewhere_ = false;
        // 关闭后 base 需重录填补菜单覆盖区，否则旧菜单像素残留 ghost。
        // 对齐 LayerView::deactivate 的 base->markAllDirty() 模式。
        View *root = this;
        while (root->parent()) root = root->parent();
        root->markAllDirty();
        clearAllDirtySubtree();    // 防残留脏标记卡死主循环
    }

protected:
    // 关闭态：层已注销，base 树遍历到本节点时直接清脏空转（不绘制菜单）
    void draw(Graphics &g) override {
        if (!registered_) {
            clearAllDirtySubtree();
            return;
        }
        View::draw(g);
    }

    // 全屏命中：LayerStack hitTest 层优先 → 菜单开启期间吞下全部点击。
    // 键盘事件（ESC）在无聚焦控件时经 hitTest(0,0) 同样到达本层。
    EventTarget *hitTest(Point point) override {
        if (!registered_ || !props.visible) return nullptr;
        return this;
    }

    bool onEvent(const DispatchEvent &event) override {
        if (!registered_) return false;
        // 坐标统一换算为相对 owner 触发区左上（与旧 Dropdown::onEvent 一致）
        float lx = event.globalX - owner_.frame.x;
        float ly = event.globalY - owner_.frame.y;

        switch (event.type) {
        case DispatchEvent::Type::Tap:
            // 命中菜单项 → 提交选中（内部会关闭菜单）；否则点外部/触发区 → 关闭。
            // 吞掉事件，不冒泡到 base（原生 select 行为）。
            {
                int idx = hitItem(lx, ly);
                if (idx >= 0) {
                    owner_.commitSelection(idx);
                } else {
                    owner_.setOpen(false);
                }
            }
            return true;

        case DispatchEvent::Type::HoverMove: {
            // 悬停高亮：变化时自脏（层 frame=菜单区，自脏即覆盖整个滚动区）
            int prev = hoveredIndex_;
            hoveredIndex_ = hitItem(lx, ly);
            if (hoveredIndex_ != prev) markDirty();
            return false;    // 不吞没, 继续冒泡
        }

        case DispatchEvent::Type::Scroll: {
            const auto &dp = owner_.dropdownProps();
            if (dp.items.empty()) return true;
            float totalH = (float)dp.items.size() * dp.itemHeight;
            float visibleH = (float)dp.maxVisibleItems * dp.itemHeight;
            float maxScroll = std::max(0.0f, totalH - visibleH);
            // 滚轮 delta（win32: WHEEL_DELTA/120，每格=1.0）。裸加仅 1px/格 → 过慢。
            // 乘 itemHeight 使一格滚动一项，对齐 ListLayout::kFactor(-30) 体感；
            // 负号修正方向：滚轮向上 → 列表向上（原生菜单 / ListLayout 语义）。
            const float kFactor = -dp.itemHeight;
            scrollOffset_ = std::clamp(scrollOffset_ + event.scrollY * kFactor, 0.0f, maxScroll);
            markDirty();
            return true;
        }

        case DispatchEvent::Type::KeyAction:
            if (event.keyCode == 27) {    // ESC 关闭菜单
                owner_.setOpen(false);
                return true;
            }
            return false;

        default: break;
        }
        return false;
    }

    void onDraw(Graphics &graphics) override {
        const auto &dp = owner_.dropdownProps();
        if (dp.items.empty()) return;
        Rect menu = frame;    // open/onLayout 已同步 = owner_.menuRect()
        float yCursor = menu.y;
        int maxN = dp.maxVisibleItems;
        float totalH = (float)maxN * dp.itemHeight;
        int skipItems = (int)(scrollOffset_ / dp.itemHeight);
        float subOff = scrollOffset_ - (float)skipItems * dp.itemHeight;

        // 整体背景
        Rect menuFull{menu.x, menu.y, menu.width, totalH};
        graphics.drawRoundedRect(menuFull, 6.0f, dp.menuBackground);
        graphics.clipRoundedRect(menuFull, 6.0f);

        auto &pipe = TextRenderPipeline::instance();
        FontId fid = pipe.activeFont();
        float fontSize = dp.fontSize;
        TextLayoutConfig itemCfg;
        itemCfg.maxWidth = menu.width - 12;
        yCursor -= subOff;
        for (int vi = 0; vi <= maxN; ++vi) {
            int i = skipItems + vi;
            if (i < 0 || i >= (int)dp.items.size()) continue;
            if (i == dp.selectedIndex || i == hoveredIndex_) {
                Rect itemRect{menu.x, yCursor, menu.width, dp.itemHeight};
                Color hl = (i == dp.selectedIndex) ? dp.selectedBackground : dp.hoverBackground;
                graphics.drawRoundedRect(itemRect, 0.0f, hl);
            }
            auto itemResult = pipe.layoutText(dp.items[i], fid, fontSize, itemCfg);
            pipe.ensureGlyphs(*itemResult);
            float itemTextY = yCursor + (dp.itemHeight - itemResult->totalHeight) * 0.5f;
            graphics.save();
            graphics.translate(menu.x + 12, itemTextY);
            graphics.drawTextCached(itemResult->glyphs, dp.textColor);
            graphics.restore();
            yCursor += dp.itemHeight;
        }
        graphics.resetClip();

        // 描边
        graphics.drawRoundedRectStroke(menuFull, 6.0f, {203, 213, 225, 255}, 1.0f);

        // 滚动条
        if ((int)dp.items.size() > maxN) {
            float barH = totalH * totalH / ((float)dp.items.size() * dp.itemHeight);
            float barY = menu.y + scrollOffset_ * totalH / ((float)dp.items.size() * dp.itemHeight);
            Rect barRect{menu.x + menu.width - 4.0f, barY, 3.0f, std::max(barH, 8.0f)};
            graphics.drawRoundedRect(barRect, 1.5f, {180, 180, 180, 180});
        }
    }

private:
    /** @brief 命中菜单项：localX/Y 相对 owner 触发区左上，返回真实索引或 -1 */
    int hitItem(float localX, float localY) const {
        const auto &dp = owner_.dropdownProps();
        if (dp.items.empty()) return -1;
        Rect menu = owner_.menuRect();    // 全局坐标
        float gx = localX + owner_.frame.x;
        float gy = localY + owner_.frame.y;
        if (gy < menu.y || gy >= menu.y + menu.height) return -1;
        if (gx < menu.x || gx >= menu.x + menu.width) return -1;
        int visualIdx = (int)((gy - menu.y) / dp.itemHeight);
        int realIdx = visualIdx + (int)(scrollOffset_ / dp.itemHeight);
        int maxN = std::min((int)dp.items.size(), dp.maxVisibleItems);
        if (visualIdx < 0 || visualIdx >= maxN) return -1;
        if (realIdx < 0 || realIdx >= (int)dp.items.size()) return -1;
        return realIdx;
    }

    Dropdown &owner_;            // 关联的触发控件（非拥有）
    bool registered_ = false;    // 是否已注册进 LayerStack
    int hoveredIndex_ = -1;      // 当前悬停的菜单项 (-1=无)
    float scrollOffset_ = 0;     // 菜单滚动偏移 (px)
};

}    // namespace

// ════════════════════════════════════════════════════════
// 辅助
// ════════════════════════════════════════════════════════
float Dropdown::menuHeight() const {
    if (dp_.items.empty()) return 0;
    int n = std::min((int)dp_.items.size(), dp_.maxVisibleItems);
    return (float)n * dp_.itemHeight;
}

Rect Dropdown::menuRect() const {
    float contentX = frame.x + props.padding.left;
    float contentY = frame.y + frame.height;
    float contentW = frame.width - props.padding.horizontal();
    return {contentX, contentY, contentW, menuHeight()};
}

// ════════════════════════════════════════════════════════
// onMeasure — 仅返回触发区高度, 菜单不占布局空间
// ════════════════════════════════════════════════════════
Size Dropdown::onMeasure(Constraints constraints) {
    float w = props.width.has_value() ? *props.width : constraints.maxWidth;
    float h = dp_.itemHeight + props.padding.vertical();
    if (props.height.has_value()) h = *props.height;
    return constraints.constrain({w, h});
}

// ════════════════════════════════════════════════════════
// onLayout — 同步菜单层 frame（菜单不参与常规子节点布局）
// ════════════════════════════════════════════════════════
void Dropdown::onLayout() {
    if (menuLayer_) menuLayer_->frame = menuRect();
}

// ════════════════════════════════════════════════════════
// setOpen / selectItem / commitSelection
// ════════════════════════════════════════════════════════
void Dropdown::setOpen(bool open) {
    if (open_ == open) return;
    open_ = open;
    if (open) {
        // 首次展开才创建菜单层（MenuView 定义在本 TU，模块内联构造器中无法实例化）
        if (!menuLayer_) {
            auto menu = std::make_unique<MenuView>(*this);
            menuLayer_ = menu.get();
            addChild(std::move(menu));    // 所有权归 children（parent 链供脏冒泡到 base 根）
        }
        // menuLayer_ 静态类型为 View*（模块接口不暴露 MenuView），本 TU 内保证
        // 其动态类型恒为 MenuView → static_cast 下转型安全。
        static_cast<MenuView *>(menuLayer_)->open();
    } else {
        if (menuLayer_) static_cast<MenuView *>(menuLayer_)->close();
    }
}

void Dropdown::selectItem(int index) {
    if (index < 0 || index >= (int)dp_.items.size()) return;
    dp_.selectedIndex = index;
    setOpen(false);
}

void Dropdown::commitSelection(int index) {
    if (index < 0 || index >= (int)dp_.items.size()) return;
    selectItem(index);
    if (binding_) binding_->setString(bindKey_, dp_.items[index]);
    fireChange();
}

// ════════════════════════════════════════════════════════
// onEvent (接入 DispatchEvent)
// ════════════════════════════════════════════════════════
bool Dropdown::onEvent(const DispatchEvent &event) {
    // 菜单开启期间所有事件被 MenuView 层吞下（LayerStack hitTest 层优先），
    // 此处仅处理关闭态：点击触发区展开。
    if (event.type == DispatchEvent::Type::Tap && !open_) {
        setOpen(true);
        return true;
    }
    return View::onEvent(event);
}

// ════════════════════════════════════════════════════════
// onDraw — TextRenderPipeline 排版渲染（仅触发区；菜单由 MenuView 层绘制）
// ════════════════════════════════════════════════════════
void Dropdown::onDraw(Graphics &graphics) {
    View::onDraw(graphics);

    auto &pipe = TextRenderPipeline::instance();
    FontId fid = pipe.activeFont();
    float fontSize = dp_.fontSize;
    Rect inner = frame.inset(props.padding.left, props.padding.top, props.padding.right, props.padding.bottom);

    // ── 展开时蓝色聚焦边框 ──
    if (open_) { graphics.drawRoundedRectStroke(frame, props.borderRadius, {66, 133, 244, 255}, 2.0f); }

    // ── ① 触发区内容 (限定在 inner 内) ──
    graphics.clipRoundedRect(inner, props.borderRadius);

    TextLayoutConfig cfg;
    cfg.maxWidth = inner.width - 16;    // 左右留 8px padding

    if (dp_.selectedIndex < 0) {
        // 占位符
        auto placeholderResult = pipe.layoutText(dp_.placeholder, fid, fontSize, cfg);
        pipe.ensureGlyphs(*placeholderResult);
        float textY = inner.y + (inner.height - placeholderResult->totalHeight) * 0.5f;
        graphics.save();
        graphics.translate(inner.x + 8, textY);
        graphics.drawTextCached(placeholderResult->glyphs, dp_.placeholderColor);
        graphics.restore();
    } else {
        // 已选中项
        std::string display = dp_.items[dp_.selectedIndex];
        if (!triggerResult_ || !triggerResult_->matchesKey(display, fid, fontSize, cfg)) {
            triggerResult_ = pipe.layoutText(display, fid, fontSize, cfg);
        }
        pipe.ensureGlyphs(*triggerResult_);
        float textY = inner.y + (inner.height - triggerResult_->totalHeight) * 0.5f;
        graphics.save();
        graphics.translate(inner.x + 8, textY);
        graphics.drawTextCached(triggerResult_->glyphs, dp_.textColor);
        graphics.restore();
    }

    // 箭头 ▼
    TextLayoutConfig arrowCfg;
    arrowCfg.maxWidth = 30;
    auto arrowResult = pipe.layoutText("\xE2\x96\xBC", fid, fontSize * 0.85f, arrowCfg);
    pipe.ensureGlyphs(*arrowResult);
    float arrowX = inner.x + inner.width - 20;
    float arrowY = inner.y + (inner.height - arrowResult->totalHeight) * 0.5f;
    graphics.save();
    graphics.translate(arrowX, arrowY);
    graphics.drawTextCached(arrowResult->glyphs, dp_.arrowColor);
    graphics.restore();

    graphics.resetClip();
}

// ════════════════════════════════════════════════════════
// fireChange
// ════════════════════════════════════════════════════════
void Dropdown::fireChange() {
    // 引擎中立回调: JS 侧收到 { value: string, index: number }
    if (handlers.onChange) {
        handlers.onChange(ChangeArgs{TypedProp{dp_.items[dp_.selectedIndex]}, dp_.selectedIndex});
    }
}

// ════════════════════════════════════════════════════════
// getProperty / setProperty — PropBus 支持
// ════════════════════════════════════════════════════════
std::string Dropdown::getProperty(const char *name) const {
    if (std::strcmp(name, "value") == 0) {
        if (dp_.selectedIndex >= 0 && dp_.selectedIndex < (int)dp_.items.size()) return dp_.items[dp_.selectedIndex];
        return "";
    }
    if (std::strcmp(name, "index") == 0) { return std::to_string(dp_.selectedIndex); }
    return View::getProperty(name);
}

// ============================================================================
// setPropertyTyped — 属性写入唯一入口（value/index）
// ============================================================================
bool Dropdown::setPropertyTyped(const char *name, const TypedProp &value) {
    if (std::strcmp(name, "value") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            for (int i = 0; i < (int)dp_.items.size(); ++i) {
                if (dp_.items[i] == *s) {
                    selectItem(i);
                    markDirty();
                    return true;
                }
            }
        }
        return false;
    }
    if (std::strcmp(name, "index") == 0) {
        auto f = typedToFloat(value);    // int64/double/数字串均可
        if (!f) { return false; }
        int idx = static_cast<int>(*f);
        if (idx >= -1 && idx < (int)dp_.items.size()) {
            if (idx == -1) {
                dp_.selectedIndex = -1;
            } else {
                selectItem(idx);
            }
            markDirty();
            return true;
        }
        return false;
    }
    return View::setPropertyTyped(name, value);
}

void Dropdown::resolveThemeDefaults() {
    auto &t = theme();
    auto &tokens = props.themeTokens;
    auto c = [&](const std::string &p, Color &v) {
        auto it = tokens.find(p);
        if (it != tokens.end() && t.resolveToken(it->second)) {
            v = *t.resolveToken(it->second);
            return true;
        }
        return false;
    };
    if (!c("textColor", dp_.textColor))
        if (dp_.textColor.isTransparent()) dp_.textColor = t.colors.onSurface;
    if (!c("placeholderColor", dp_.placeholderColor))
        if (dp_.placeholderColor.isTransparent()) dp_.placeholderColor = t.colors.onSurfaceVariant;
    if (!c("arrowColor", dp_.arrowColor))
        if (dp_.arrowColor.isTransparent()) dp_.arrowColor = t.colors.onSurfaceVariant;
    if (!c("menuBackground", dp_.menuBackground))
        if (dp_.menuBackground.isTransparent()) dp_.menuBackground = t.colors.surface;
    if (!c("hoverBackground", dp_.hoverBackground))
        if (dp_.hoverBackground.isTransparent()) dp_.hoverBackground = t.colors.surfaceVariant;
    if (!c("selectedBackground", dp_.selectedBackground))
        if (dp_.selectedBackground.isTransparent()) dp_.selectedBackground = t.colors.surfaceVariant;
}