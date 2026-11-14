add_library(kwik_bridge)
target_sources(kwik_bridge
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules/bridge
        FILES
            modules/bridge/props_parser.cppm
            modules/bridge/props_reflect.cppm    # C++26 反射桥（纯模板，无 .cpp；-freflection 随 GNU）
            modules/bridge/element_parser.cppm
            modules/bridge/prop_bus.cppm
            modules/bridge/binding_registry.cppm
            modules/bridge/bindings.cppm
            modules/bridge/theme_bridge.cppm
            modules/bridge/event_adapter.cppm
            modules/bridge/js_table_data_source.cppm
            modules/bridge/js_lazy_list_source.cppm
            modules/bridge/element_spec.cppm
            modules/bridge/g2d.cppm
            modules/bridge/expansion.cppm
    
    PRIVATE
        src/bridge/props_parser.cpp
        src/bridge/element_parser.cpp
        src/bridge/prop_bus.cpp
        src/bridge/binding_registry.cpp
        src/bridge/bindings.cpp
        src/bridge/theme_bridge.cpp
        src/bridge/event_adapter.cpp
        src/bridge/js_table_data_source.cpp
        src/bridge/js_lazy_list_source.cpp
        src/bridge/element_spec.cpp
        src/bridge/g2d.cpp
        src/bridge/expansion.cpp
)
target_link_libraries(kwik_bridge
    PRIVATE
        kwik_core       # Color, Size, Rect, EdgeInsets, Shadow
        kwik_element    # View, ViewProps, BorderStyle
        kwik_engine     # JSValueRef
        qjs
)
# C++26 静态反射（P2996）仅 GCC16 支持，且需 -freflection 显式开启；
# PUBLIC——import 本模块者（模板实例化含 splice）也需要该旗标。
# 不放全局：三方目标（harfbuzz 自设低标准）会被全局旗标打崩
target_compile_options(kwik_bridge
    PUBLIC "$<$<CXX_COMPILER_ID:GNU>:-freflection>")

# 6. 添加编译定义（COMPILE_DEFINITIONS）
target_compile_definitions(kwik_bridge
    PRIVATE
        KWIK_BRIDGE_MODULE
)

install(TARGETS kwik_bridge
    EXPORT KwiKUITargets
    FILE_SET cxx_modules DESTINATION share/kwik-ui/modules/bridge
    ARCHIVE DESTINATION lib
)