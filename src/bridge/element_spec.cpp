// ============================================================================
// element_spec.cpp — 元素扩展注册表实现 (Meyer's Singleton)
// ============================================================================
module;
module kwik.bridge.element_spec;

ElementRegistry &ElementRegistry::instance() {
    static ElementRegistry reg;    // 函数局部静态: 首次调用构造, 线程安全
    return reg;
}

void ElementRegistry::registerElement(ElementSpec spec) {
    specs_[spec.typeName] = std::move(spec);
}

const ElementSpec *ElementRegistry::find(std::string_view typeName) const {
    auto it = specs_.find(std::string(typeName));
    return it == specs_.end() ? nullptr : &it->second;
}

const std::unordered_map<std::string, ElementSpec> &ElementRegistry::specs() const {
    return specs_;
}