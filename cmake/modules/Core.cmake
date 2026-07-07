
# 1. 创建库目标
add_library(kwik_core)


# 2. 添加C++20模块接口文件（PUBLIC_MODULES）
target_sources(kwik_core
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_SOURCE_DIR}/modules/core
        FILES
            modules/core/types.cppm
            modules/core/constraints.cppm
            modules/core/log.cppm
            modules/core/task_queue.cppm       
            modules/core/thread_pool.cppm    
            modules/core/coroutine.cppm
            modules/core/scheduler.cppm
            modules/core/props.cppm
            modules/core/prop_meta.cppm
    PRIVATE
        src/core/log.cpp
        src/core/scheduler.cpp
        src/core/prop_meta.cpp
)

# 4. 添加编译定义（COMPILE_DEFINITIONS）
target_compile_definitions(kwik_core PRIVATE KWIK_CORE_MODULE)

install(TARGETS kwik_core
    EXPORT KwiKUITargets
    FILE_SET cxx_modules DESTINATION share/kwik-ui/modules/core
    ARCHIVE DESTINATION lib
)
