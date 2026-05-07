add_library(kwik_event)
target_sources(kwik_event
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules/event
        FILES
            modules/event/event.cppm
    PRIVATE
        src/event/event.cpp
)
target_link_libraries(kwik_event
    PRIVATE
        kwik_core
        kwik_platform
        kwik_element
        qjs
)
target_compile_definitions(kwik_event
    PRIVATE
        KWIK_EVENT_MODULE
)