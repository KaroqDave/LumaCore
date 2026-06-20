qt_add_executable(lumacore-daemon
    MANUAL_FINALIZATION
    DaemonOptions.cpp
    DaemonOptions.h
    ${PROJECT_SOURCE_DIR}/backends/auto/AutoBackend.cpp
    ${PROJECT_SOURCE_DIR}/backends/auto/AutoBackend.h
    main.cpp
)

target_include_directories(lumacore-daemon
    PRIVATE
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(lumacore-daemon
    PRIVATE
        lumacore_core
        lumacore_ipc
        Qt6::Core
        Qt6::Gui
        Qt6::Network
)

if(TARGET lumacore_linux_backend)
    target_compile_definitions(lumacore-daemon PRIVATE LUMACORE_HAS_LINUX_DISCOVERY)
    target_link_libraries(lumacore-daemon PRIVATE lumacore_linux_backend)
endif()

if(TARGET lumacore_asus_aura_backend)
    target_compile_definitions(lumacore-daemon PRIVATE LUMACORE_HAS_ASUS_AURA_HID)
    target_link_libraries(lumacore-daemon PRIVATE lumacore_asus_aura_backend)
endif()
