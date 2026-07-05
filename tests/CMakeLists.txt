# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
    get_filename_component(LUMACORE_COMPILER_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
endif()

function(lumacore_set_windows_test_runtime_path test_name)
    if(WIN32)
        set_property(
            TEST "${test_name}"
            PROPERTY ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
                "PATH=path_list_prepend:${LUMACORE_COMPILER_BIN_DIR}"
        )
    endif()
endfunction()

function(lumacore_add_test_executable target test_name)
    qt_add_executable(${target} ${ARGN})
    add_test(NAME ${test_name} COMMAND ${target})
    lumacore_set_windows_test_runtime_path("${test_name}")
endfunction()

lumacore_add_test_executable(lumacore_device_manager_write_gate_test device_manager_write_gate
    DeviceManagerWriteGateTest.cpp
)
target_link_libraries(lumacore_device_manager_write_gate_test PRIVATE lumacore_core Qt6::Core)

lumacore_add_test_executable(lumacore_profile_persistence_test profile_persistence
    ProfilePersistenceTest.cpp
)
target_link_libraries(lumacore_profile_persistence_test PRIVATE lumacore_core Qt6::Core)

lumacore_add_test_executable(lumacore_profile_store_import_test profile_store_import
    ProfileStoreImportTest.cpp
)
target_link_libraries(lumacore_profile_store_import_test PRIVATE lumacore_core Qt6::Core)

lumacore_add_test_executable(lumacore_effects_engine_test effects_engine
    EffectsEngineTest.cpp
)
target_link_libraries(lumacore_effects_engine_test PRIVATE lumacore_core Qt6::Core)

lumacore_add_test_executable(lumacore_auto_backend_test auto_backend
    AutoBackendTest.cpp
    ${PROJECT_SOURCE_DIR}/backends/auto/AutoBackend.cpp
    ${PROJECT_SOURCE_DIR}/backends/auto/AutoBackend.h
)
target_include_directories(lumacore_auto_backend_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(lumacore_auto_backend_test PRIVATE lumacore_core Qt6::Core)

lumacore_add_test_executable(lumacore_probe_result_test probe_result
    ProbeResultTest.cpp
    ${PROJECT_SOURCE_DIR}/backends/windows/WindowsDiscoveryBackend.cpp
    ${PROJECT_SOURCE_DIR}/backends/windows/WindowsDiscoveryBackend.h
    ${PROJECT_SOURCE_DIR}/backends/windows/WindowsDiscoveredDevice.cpp
    ${PROJECT_SOURCE_DIR}/backends/windows/WindowsDiscoveredDevice.h
    ${PROJECT_SOURCE_DIR}/hardware/linux/ProbeResult.cpp
    ${PROJECT_SOURCE_DIR}/hardware/linux/ProbeResult.h
    ${PROJECT_SOURCE_DIR}/hardware/common/ProbeResultCommon.cpp
    ${PROJECT_SOURCE_DIR}/hardware/common/ProbeResultCommon.h
    ${PROJECT_SOURCE_DIR}/hardware/common/ProbeTypes.h
    ${PROJECT_SOURCE_DIR}/hardware/windows/HidapiProbe.cpp
    ${PROJECT_SOURCE_DIR}/hardware/windows/HidapiProbe.h
    ${PROJECT_SOURCE_DIR}/hardware/windows/ProbeResult.cpp
    ${PROJECT_SOURCE_DIR}/hardware/windows/ProbeResult.h
    ${PROJECT_SOURCE_DIR}/hardware/windows/HidDeviceWriter.cpp
    ${PROJECT_SOURCE_DIR}/hardware/windows/HidDeviceWriter.h
)
target_include_directories(lumacore_probe_result_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(lumacore_probe_result_test PRIVATE lumacore_core Qt6::Core)

lumacore_add_test_executable(lumacore_daemon_frame_codec_test daemon_frame_codec
    DaemonFrameCodecTest.cpp
)
target_include_directories(lumacore_daemon_frame_codec_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(lumacore_daemon_frame_codec_test PRIVATE lumacore_ipc Qt6::Core)

lumacore_add_test_executable(lumacore_daemon_protocol_test daemon_protocol
    DaemonProtocolTest.cpp
)
target_link_libraries(lumacore_daemon_protocol_test
    PRIVATE
        lumacore_core
        lumacore_ipc
        lumacore_daemon_backend
        Qt6::Core
)

lumacore_add_test_executable(lumacore_daemon_server_failure_path_test daemon_server_failure_path
    DaemonServerFailurePathTest.cpp
)
target_include_directories(lumacore_daemon_server_failure_path_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(lumacore_daemon_server_failure_path_test PRIVATE lumacore_core lumacore_ipc Qt6::Core)

lumacore_add_test_executable(lumacore_daemon_write_dispatch_test daemon_write_dispatch
    DaemonWriteDispatchTest.cpp
)
target_include_directories(lumacore_daemon_write_dispatch_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(lumacore_daemon_write_dispatch_test PRIVATE lumacore_core lumacore_ipc Qt6::Core)

lumacore_add_test_executable(lumacore_option_parsing_test option_parsing
    OptionParsingTest.cpp
    ${PROJECT_SOURCE_DIR}/app/GuiOptions.cpp
    ${PROJECT_SOURCE_DIR}/app/GuiOptions.h
    ${PROJECT_SOURCE_DIR}/daemon/DaemonOptions.cpp
    ${PROJECT_SOURCE_DIR}/daemon/DaemonOptions.h
)
target_include_directories(lumacore_option_parsing_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(lumacore_option_parsing_test PRIVATE lumacore_ipc Qt6::Core)

lumacore_add_test_executable(lumacore_settings_controller_test settings_controller
    SettingsControllerTest.cpp
)
target_link_libraries(lumacore_settings_controller_test PRIVATE lumacore_ui Qt6::Core)

lumacore_add_test_executable(lumacore_profile_schedule_runner_test profile_schedule_runner
    ProfileScheduleRunnerTest.cpp
    ${PROJECT_SOURCE_DIR}/app/ProfileScheduleRunner.cpp
    ${PROJECT_SOURCE_DIR}/app/ProfileScheduleRunner.h
)
target_include_directories(lumacore_profile_schedule_runner_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(lumacore_profile_schedule_runner_test PRIVATE lumacore_ui Qt6::Core)

lumacore_add_test_executable(lumacore_app_controller_test app_controller
    AppControllerTest.cpp
)
target_link_libraries(lumacore_app_controller_test PRIVATE lumacore_ui Qt6::Core)

lumacore_add_test_executable(lumacore_device_tree_model_test device_tree_model
    DeviceTreeModelTest.cpp
)
target_link_libraries(lumacore_device_tree_model_test PRIVATE lumacore_ui lumacore_core Qt6::Core)

lumacore_add_test_executable(lumacore_global_operation_report_test global_operation_report
    GlobalOperationReportTest.cpp
)
target_link_libraries(lumacore_global_operation_report_test PRIVATE lumacore_ui lumacore_core Qt6::Core)

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
add_dependencies(
    lumacore_daemon_launcher_test
    lumacore-daemon
    lumacore_daemon_launch_exit_fixture
    lumacore_daemon_launch_sleep_fixture
)
add_test(
    NAME daemon_launcher
    COMMAND lumacore_daemon_launcher_test
        $<TARGET_FILE:lumacore-daemon>
        $<TARGET_FILE:lumacore_daemon_launch_exit_fixture>
        $<TARGET_FILE:lumacore_daemon_launch_sleep_fixture>
)
lumacore_set_windows_test_runtime_path(daemon_launcher)

qt_add_executable(lumacore_daemon_end_to_end_test DaemonEndToEndTest.cpp)
target_include_directories(lumacore_daemon_end_to_end_test PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(lumacore_daemon_end_to_end_test PRIVATE lumacore_core lumacore_ipc Qt6::Core)
add_dependencies(lumacore_daemon_end_to_end_test lumacore-daemon)
add_test(
    NAME daemon_end_to_end
    COMMAND lumacore_daemon_end_to_end_test $<TARGET_FILE:lumacore-daemon>
)
lumacore_set_windows_test_runtime_path(daemon_end_to_end)

# Headless smoke test: drive the real lumacore GUI's --self-test path, which
# loads Main.qml with the production bindings and fails on any QML warning. The
# offscreen platform lets it run without a display in CI.
add_test(
    NAME qml_smoke
    COMMAND lumacore --self-test
)
set_property(TEST qml_smoke PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
lumacore_set_windows_test_runtime_path(qml_smoke)

if(TARGET lumacore_asus_aura_backend)
    lumacore_add_test_executable(lumacore_asus_aura_backend_test asus_aura_hid_backend
        AsusAuraHidBackendTest.cpp
    )
    target_link_libraries(lumacore_asus_aura_backend_test PRIVATE lumacore_asus_aura_backend Qt6::Core)

    lumacore_add_test_executable(lumacore_asus_aura_protocol_test asus_aura_hid_protocol
        AsusAuraHidProtocolTest.cpp
        ${PROJECT_SOURCE_DIR}/hardware/asus/AsusAuraHidProtocol.cpp
        ${PROJECT_SOURCE_DIR}/hardware/asus/AsusAuraHidProtocol.h
    )
    target_link_libraries(lumacore_asus_aura_protocol_test PRIVATE lumacore_core Qt6::Core)
endif()
