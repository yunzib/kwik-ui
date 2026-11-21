export module kwik.layout.flex_layout;
import kwik.element.view;
import kwik.core.props;
import kwik.core.props_compare;    // structPropsEqual — P1a applyXxxProps 精准标脏
import kwik.core.types;
import kwik.core.constraints;
import std;
export class FlexLayout : public View {
public:
    FlexLayout() = default;
    explicit FlexLayout(ViewProps p, ContainerProps cp = {}) : View(std::move(p)), container_(std::move(cp)) {
        if (props.background.a == 0) { props.background = Color::transparent(); }
    }
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    float getTotalMainSize() const;

    ElementType type() const override {
        return ElementType::FlexLayout;
    }

    /**
     * @brief 增量更新容器属性（reconcile 路径）
     * @param cp 重新解析的容器属性（direction/gap/flexWrap/justifyContent/...）
     */
    void applyContainerProps(const ContainerProps &cp) {
        // P1a：与现值全等零动作（容器属性低频变，此前无条件覆写靠全树标脏兜底）
        if (structPropsEqual(cp, container_)) return;
        container_ = cp;
        markDirty();
        requestLayout();
    }

private:
    ContainerProps container_;
};