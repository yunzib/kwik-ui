
# 1. 创建库目标
add_library(kwik_engine)

# 2. 添加C++20模块接口文件（PUBLIC_MODULES）
target_sources(kwik_engine
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules/engine
        FILES
            modules/engine/quickjs_runtime.cppm
            modules/engine/quickjs_context.cppm
            modules/engine/js_value.cppm
            modules/engine/bindings.cppm
            modules/engine/state_binding.cppm
            modules/engine/channel.cppm
    
    # 添加私有实现源文件（PRIVATE_SOURCES）
    PRIVATE
        src/engine/quickjs_runtime.cpp
        src/engine/quickjs_context.cpp
        src/engine/js_value.cpp
        src/engine/bindings.cpp
        src/engine/state_binding.cpp
        src/engine/channel.cpp 


)


# 5. 添加依赖（DEPENDS: kwik_core）
target_link_libraries(kwik_engine
    PRIVATE
        kwik_core
)

# 6. 添加链接库（LINK_LIBRARIES: quickjs）
target_link_libraries(kwik_engine
    PRIVATE
        qjs
)

# 7. 添加编译定义（COMPILE_DEFINITIONS）
target_compile_definitions(kwik_engine
    PRIVATE
        KWIK_ENGINE_MODULE
)
