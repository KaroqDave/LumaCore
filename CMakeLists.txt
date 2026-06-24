cmake_minimum_required(VERSION 3.24)

project(LumaCore
    VERSION 1.1.6.8
    DESCRIPTION "Linux-first RGB control application"
    LANGUAGES CXX
)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 QuickDialogs2 Widgets)
find_package(PkgConfig QUIET)

include(CheckIncludeFileCXX)

qt_standard_project_setup(REQUIRES 6.5)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8.0)
    qt_policy(SET QTP0004 NEW)
endif()

set(LUMACORE_LINUX_DEFAULT OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LUMACORE_LINUX_DEFAULT ON)
endif()

option(LUMACORE_ENABLE_LINUX_DISCOVERY "Build daemon-only Linux read-only hardware discovery backend" ${LUMACORE_LINUX_DEFAULT})
option(LUMACORE_ENABLE_HIDAPI "Enable read-only hidapi discovery when available" ON)
option(LUMACORE_ENABLE_LIBUSB "Enable read-only libusb discovery when available" ON)
option(LUMACORE_ENABLE_I2C_DEV "Enable optional read-only i2c-dev adapter metadata discovery" OFF)
option(LUMACORE_ENABLE_ASUS_AURA_HID "Build ASUS Aura USB HID backend with config-verified confirmation-gated writes" ${LUMACORE_LINUX_DEFAULT})
option(LUMACORE_ENABLE_WARNINGS "Enable project compiler warnings for first-party targets" ON)
option(LUMACORE_WARNINGS_AS_ERRORS "Treat project compiler warnings as errors" OFF)
option(LUMACORE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer for supported Linux compiler builds" OFF)
option(LUMACORE_INSTALL_SYSTEMD_UNIT "Install the lumacore-daemon systemd unit on Linux" ${LUMACORE_LINUX_DEFAULT})
set(
    LUMACORE_SYSTEMD_UNIT_DIR
    "lib/systemd/system"
    CACHE STRING
    "Install destination for the lumacore-daemon systemd unit. Relative paths are resolved under the install prefix."
)

set(LUMACORE_HAS_HIDAPI OFF)
set(LUMACORE_HAS_LIBUSB OFF)
set(LUMACORE_HAS_I2C_DEV OFF)

add_library(lumacore_project_options INTERFACE)

if(LUMACORE_ENABLE_WARNINGS)
    target_compile_options(lumacore_project_options
        INTERFACE
            $<$<CXX_COMPILER_ID:MSVC>:/W4>
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic>
    )
endif()

if(LUMACORE_WARNINGS_AS_ERRORS)
    target_compile_options(lumacore_project_options
        INTERFACE
            $<$<CXX_COMPILER_ID:MSVC>:/WX>
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Werror>
    )
endif()

if(LUMACORE_ENABLE_SANITIZERS)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(lumacore_project_options
            INTERFACE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
        )
        target_link_options(lumacore_project_options
            INTERFACE
                -fsanitize=address,undefined
        )
    else()
        message(WARNING "LUMACORE_ENABLE_SANITIZERS is only supported for GNU/Clang Linux builds.")
    endif()
endif()

function(lumacore_apply_project_options)
    foreach(_lumacore_target IN LISTS ARGN)
        if(TARGET "${_lumacore_target}")
            target_link_libraries("${_lumacore_target}" PRIVATE lumacore_project_options)
        endif()
    endforeach()
endfunction()

if(LUMACORE_ENABLE_LINUX_DISCOVERY AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(LUMACORE_ENABLE_HIDAPI AND PkgConfig_FOUND)
        pkg_check_modules(HIDAPI_HIDRAW QUIET IMPORTED_TARGET hidapi-hidraw)
        pkg_check_modules(HIDAPI_LIBUSB QUIET IMPORTED_TARGET hidapi-libusb)
        if(TARGET PkgConfig::HIDAPI_HIDRAW)
            set(LUMACORE_HAS_HIDAPI ON)
            set(LUMACORE_HIDAPI_TARGET PkgConfig::HIDAPI_HIDRAW)
        elseif(TARGET PkgConfig::HIDAPI_LIBUSB)
            set(LUMACORE_HAS_HIDAPI ON)
            set(LUMACORE_HIDAPI_TARGET PkgConfig::HIDAPI_LIBUSB)
        endif()
    endif()

    if(LUMACORE_ENABLE_LIBUSB AND PkgConfig_FOUND)
        pkg_check_modules(LIBUSB QUIET IMPORTED_TARGET libusb-1.0)
        if(TARGET PkgConfig::LIBUSB)
            set(LUMACORE_HAS_LIBUSB ON)
            set(LUMACORE_LIBUSB_TARGET PkgConfig::LIBUSB)
        endif()
    endif()

    if(LUMACORE_ENABLE_I2C_DEV)
        check_include_file_cxx("linux/i2c-dev.h" LUMACORE_HAS_I2C_DEV)
    endif()
endif()

add_subdirectory(core)
add_subdirectory(ipc)
add_subdirectory(backends)
add_subdirectory(ui)

if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    get_filename_component(_lumacore_mingw_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(_lumacore_windres_wrapper "${CMAKE_BINARY_DIR}/lumacore-windres.bat")
    file(WRITE "${_lumacore_windres_wrapper}"
        "@echo off\n"
        "set \"PATH=${_lumacore_mingw_bin};%PATH%\"\n"
        "\"${_lumacore_mingw_bin}/windres.exe\" %*\n"
    )
    set(CMAKE_RC_COMPILER "${_lumacore_windres_wrapper}")
endif()

qt_add_executable(lumacore MANUAL_FINALIZATION)
add_subdirectory(app)

set_source_files_properties(ui/qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(lumacore
    URI LumaCore
    VERSION 1.0
    QML_FILES
        ui/qml/AboutDialog.qml
        ui/qml/Theme.qml
        ui/qml/components/ActivityLogPanel.qml
        ui/qml/components/AllOffConfirmationDialog.qml
        ui/qml/components/AppButton.qml
        ui/qml/components/AppSlider.qml
        ui/qml/components/AppSwitch.qml
        ui/qml/components/BackendInfoDialog.qml
        ui/qml/components/BackendStatusChip.qml
        ui/qml/components/ColorPickerDialog.qml
        ui/qml/components/DeviceTreePanel.qml
        ui/qml/components/DeviceWorkspacePanel.qml
        ui/qml/components/EffectSelector.qml
        ui/qml/components/GlobalControls.qml
        ui/qml/components/NavIcon.qml
        ui/qml/components/NavItem.qml
        ui/qml/components/NavRail.qml
        ui/qml/components/OperationResultDialog.qml
        ui/qml/components/ProfileManager.qml
        ui/qml/components/SectionCard.qml
        ui/qml/components/SettingsPage.qml
        ui/qml/components/SetupStatusPanel.qml
        ui/qml/components/ZoneEditor.qml
        ui/qml/Main.qml
)

add_subdirectory(daemon)

enable_testing()
add_subdirectory(tests)

message(STATUS "LumaCore Linux discovery: ${LUMACORE_ENABLE_LINUX_DISCOVERY}")
message(STATUS "LumaCore hidapi provider: ${LUMACORE_HAS_HIDAPI}")
message(STATUS "LumaCore libusb provider: ${LUMACORE_HAS_LIBUSB}")
message(STATUS "LumaCore i2c-dev provider: ${LUMACORE_HAS_I2C_DEV}")
message(STATUS "LumaCore ASUS Aura HID backend: ${LUMACORE_ENABLE_ASUS_AURA_HID}")

qt_finalize_executable(lumacore)
qt_finalize_executable(lumacore-daemon)

lumacore_apply_project_options(
    lumacore
    lumacore-daemon
    lumacore_core
    lumacore_ipc
    lumacore_daemon_backend
    lumacore_linux_backend
    lumacore_asus_aura_backend
    lumacore_ui
    lumacore_device_manager_write_gate_test
    lumacore_profile_persistence_test
    lumacore_auto_backend_test
    lumacore_probe_result_test
    lumacore_daemon_protocol_test
    lumacore_option_parsing_test
    lumacore_settings_controller_test
    lumacore_profile_schedule_runner_test
    lumacore_app_controller_test
    lumacore_daemon_launch_exit_fixture
    lumacore_daemon_launch_sleep_fixture
    lumacore_daemon_launcher_test
    lumacore_asus_aura_protocol_test
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LUMACORE_DESKTOP_FILE "${CMAKE_CURRENT_BINARY_DIR}/packaging/linux/lumacore.desktop")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/lumacore.desktop.in"
        "${LUMACORE_DESKTOP_FILE}"
        @ONLY
    )

    set(LUMACORE_SYSTEMD_SERVICE_FILE "${CMAKE_CURRENT_BINARY_DIR}/packaging/systemd/lumacore-daemon.service")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/systemd/lumacore-daemon.service.in"
        "${LUMACORE_SYSTEMD_SERVICE_FILE}"
        @ONLY
    )

    install(
        TARGETS lumacore lumacore-daemon
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )

    install(
        FILES "${LUMACORE_DESKTOP_FILE}"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    )

    install(
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/lumacore.svg"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
        RENAME "lumacore.svg"
    )

    foreach(_lumacore_icon_size IN ITEMS 16 32 48 64 128 256)
        install(
            FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/lumacore-${_lumacore_icon_size}.png"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_lumacore_icon_size}x${_lumacore_icon_size}/apps"
            RENAME "lumacore.png"
        )
    endforeach()

    if(LUMACORE_INSTALL_SYSTEMD_UNIT)
        install(
            FILES "${LUMACORE_SYSTEMD_SERVICE_FILE}"
            DESTINATION "${LUMACORE_SYSTEMD_UNIT_DIR}"
        )
    endif()
endif()
