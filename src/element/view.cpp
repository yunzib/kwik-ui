module kwik.element.view;

import kwik.render.graphics;
import kwik.core.types;
import kwik.core.constraints;
import std;

    // ============================================================================
    // 布局实现
    // ============================================================================
    Size View::onMeasure(Constraints constraints) {
        // 获取自身尺寸（优先使用属性值）
        float w = props.width.value_or(constraints.maxWidth);
        float h = props.height.value_or(constraints.maxHeight);
        
        // 加上内边距
        w += props.padding.horizontal();
        h += props.padding.vertical();
        
        Size contentSize = {w, h};
        
        // 如果有子控件，需要测量子控件
        if (!children.empty()) {
            Constraints childConstraints = constraints.inset(props.padding);
            
            float maxChildWidth = 0;
            float totalChildHeight = 0;
            
            for (auto& child : children) {
                Size childSize = child->measure(childConstraints);
                maxChildWidth = std::max(maxChildWidth, childSize.width);
                totalChildHeight += childSize.height;
            }
            
            // 如果尺寸未指定，根据子控件确定
            if (!props.width.has_value()) {
                w = maxChildWidth + props.padding.horizontal();
            }
            if (!props.height.has_value()) {
                h = totalChildHeight + props.padding.vertical();
            }
        }
        
        // 应用约束
        return constraints.constrain({w, h});
    }
    void View::onLayout() {
        // 简单的垂直布局：子控件从上到下排列
        float yOffset = frame.y + props.padding.top;
        float xOffset = frame.x + props.padding.left;
        float childWidth = frame.width - props.padding.horizontal();
        
        for (auto& child : children) {
            // 测量子控件
            Size childSize = child->measure(Constraints::fixed(childWidth, frame.height));
            
            // 布局子控件
            child->layout(Rect{xOffset, yOffset, childSize.width, childSize.height});
            
            // 累加Y偏移
            yOffset += childSize.height;
        }
    }
    // ============================================================================
    // 绘制实现
    // ============================================================================
    void View::draw(Graphics& graphics) {
        // 不可见则跳过
        if (!props.visible) return;
        
        onDraw(graphics);
    }
    void View::onDraw(Graphics& graphics) {
        graphics.save();
        
        // 应用透明度
        if (props.opacity < 1.0f) {
            graphics.setOpacity(props.opacity);
        }
        
        Rect drawRect = frame;
        
        // 1. 绘制阴影
        if (props.shadow.has_value()) {
            graphics.drawShadow(drawRect, props.borderRadius, *props.shadow);
        }
        
        // 2. 绘制背景
        if (props.background.isVisible()) {
            graphics.drawRoundedRect(drawRect, props.borderRadius, props.background);
        }
        
        // 3. 绘制边框
        if (props.borderWidth > 0 && props.borderStyle != BorderStyle::None) {
            graphics.drawRoundedRectStroke(drawRect, props.borderRadius, 
                                        props.borderColor, props.borderWidth);
        }
        
        // 4. 计算内容区域
        Rect contentRect = {
            frame.x + props.padding.left,
            frame.y + props.padding.top,
            frame.width - props.padding.horizontal(),
            frame.height - props.padding.vertical()
        };
        
        // 5. 裁剪内容区域
        if (props.borderRadius > 0) {
            graphics.clipRoundedRect(contentRect, props.borderRadius);
        }
        
        // 6. 绘制子控件
        for (auto& child : children) {
            child->draw(graphics);
        }
        
        graphics.restore();
    }