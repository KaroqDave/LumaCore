qt_add_executable(lumacore_device_manager_write_gate_test
    DeviceManagerWriteGateTest.cpp
)
target_link_libraries(lumacore_device_manager_write_gate_test PRIVATE lumacore_core Qt6::Core)
add_test(NAME device_manager_write_gate COMMAND lumacore_device_manager_write_gate_test)

qt_add_executable(lumacore_profile_persistence_test
    ProfilePersistenceTest.cpp
)
target_link_libraries(lumacore_profile_persistence_test PRIVATE lumacore_core Qt6::Core)
add_test(NAME profile_persistence COMMAND lumacore_profile_persistence_test)

qt_add_executable(lumacore_auto_backend_test
    AutoBackendTest.cpp
    ${PROJECT_SOURCE_DIR}/backends/auto/AutoBackend.cpp
    ${PROJECT_SOURCE_DIR}/backends/auto/AutoBackend.h
)
target_include_directories(lumacore_auto_backend_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(lumacore_auto_backend_test PRIVATE lumacore_core Qt6::Core)
add_test(NAME auto_backend COMMAND lumacore_auto_backend_test)

qt_add_executable(lumacore_daemon_protocol_test
    DaemonProtocolTest.cpp
)
target_link_libraries(lumacore_daemon_protocol_test
    PRIVATE
        lumacore_core
        lumacore_ipc
        Qt6::Core
)
add_test(NAME daemon_protocol COMMAND lumacore_daemon_protocol_test)

qt_add_executable(lumacore_option_parsing_test
    OptionParsingTest.cpp
    ${PROJECT_SOURCE_DIR}/app/GuiOptions.cpp
    ${PROJECT_SOURCE_DIR}/app/GuiOptions.h
    ${PROJECT_SOURCE_DIR}/daemon/DaemonOptions.cpp
    ${PROJECT_SOURCE_DIR}/daemon/DaemonOptions.h
)
target_include_directories(lumacore_option_parsing_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(lumacore_option_parsing_test PRIVATE lumacore_ipc Qt6::Core)
add_test(NAME option_parsing COMMAND lumacore_option_parsing_test)

qt_add_executable(lumacore_daemon_launch_exit_fixture DaemonLaunchFixture.cpp)
target_link_libraries(lumacore_daemon_launch_exit_fixture PRIVATE Qt6::Core)

qt_add_executable(lumacore_daemon_launch_sleep_fixture DaemonLaunchFixture.cpp)
target_compile_definitions(lumacore_daemon_launch_sleep_fixture PRIVATE LUMACORE_FIXTURE_SLEEP)
target_link_libraries(lumacore_daemon_launch_sleep_fixture PRIVATE Qt6::Core)

qt_add_executable(lumacore_daemon_launcher_test
    DaemonLauncherTest.cpp
    ${PROJECT_SOURCE_DIR}/app/DaemonLauncher.cpp
    ${PROJECT_SOURCE_DIR}/app/DaemonLauncher.h
)
target_include_directories(lumacore_daemon_launcher_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(lumacore_daemon_launcher_test PRIVATE lumacore_ipc Qt6::Core)
add_test(
    NAME daemon_launcher
    COMMAND lumacore_daemon_launcher_test
        $<TARGET_FILE:lumacore-daemon>
        $<TARGET_FILE:lumacore_daemon_launch_exit_fixture>
        $<TARGET_FILE:lumacore_daemon_launch_sleep_fixture>
)

if(WIN32)
    get_filename_component(LUMACORE_COMPILER_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set_property(
        TEST
            device_manager_write_gate
            profile_persistence
            auto_backend
            daemon_protocol
            option_parsing
            daemon_launcher
        PROPERTY ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
            "PATH=path_list_prepend:${LUMACORE_COMPILER_BIN_DIR}"
    )
endif()

if(TARGET lumacore_asus_aura_backend)
    qt_add_executable(lumacore_asus_aura_protocol_test
        AsusAuraHidProtocolTest.cpp
        ${PROJECT_SOURCE_DIR}/hardware/linux/AsusAuraHidProtocol.cpp
        ${PROJECT_SOURCE_DIR}/hardware/linux/AsusAuraHidProtocol.h
    )
    target_link_libraries(lumacore_asus_aura_protocol_test PRIVATE lumacore_core Qt6::Core)
    add_test(NAME asus_aura_hid_protocol COMMAND lumacore_asus_aura_protocol_test)
endif()
