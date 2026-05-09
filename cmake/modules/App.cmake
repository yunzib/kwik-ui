add_library(kwik_app)
target_sources(kwik_app
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules/app
        FILES
            modules/app/application.cppm
    PRIVATE
        src/app/application.cpp
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
target_compile_definitions(kwik_app
    PRIVATE
        KWIK_APP_MODULE
)