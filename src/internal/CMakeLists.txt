set(src
    dbusmarshal.cpp
    safe_strerror.cpp
    TraceMessageHandler.cpp
)

add_library(storage-framework-common-internal STATIC ${src})
target_link_libraries(storage-framework-common-internal
    Qt5::DBus
)
