add_library(kwik_app)
target_sources(kwik_app
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules/app
        FILES
            modules/app/application.cppm
            modules/app/kwik_runtime.cppm
            modules/app/crash_reporter.cppm
    PRIVATE
        src/app/application.cpp
        src/app/kwik_runtime.cpp
        src/app/crash_reporter.cpp
)
target_link_libraries(kwik_app
    PRIVATE
        kwik_utils
        kwik_core
        kwik_platform
        kwik_engine
        kwik_element
        kwik_render
        kwik_bridge
        kwik_event
        qjs
)
# crash_reporter 经 LoadLibrary 动态绑定 MiniDumpWriteDump，无需 dbghelp 链接
target_compile_definitions(kwik_app
    PRIVATE
        KWIK_APP_MODULE
)


install(TARGETS kwik_app
    EXPORT KwiKUITargets
    FILE_SET cxx_modules DESTINATION share/kwik-ui/modules/app
    ARCHIVE DESTINATION lib
)