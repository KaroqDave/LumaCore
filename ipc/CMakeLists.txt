qt_add_library(lumacore_ipc STATIC
    DaemonClient.cpp
    DaemonClient.h
    DaemonFrameCodec.cpp
    DaemonFrameCodec.h
    DaemonProtocol.cpp
    DaemonProtocol.h
    DaemonServer.cpp
    DaemonServer.h
)

target_include_directories(lumacore_ipc
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(lumacore_ipc
    PUBLIC
        lumacore_core
        Qt6::Network
)
