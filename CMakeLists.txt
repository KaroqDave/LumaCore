cmake_minimum_required(VERSION 3.24)

project(LumaCore
    VERSION 0.5.5
    DESCRIPTION "Linux-first RGB control application"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 QuickDialogs2)
find_package(PkgConfig QUIET)

include(CheckIncludeFileCXX)

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

set(LUMACORE_LINUX_DEFAULT OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LUMACORE_LINUX_DEFAULT ON)
endif()

option(LUMACORE_ENABLE_LINUX_DISCOVERY "Build daemon-only Linux read-only hardware discovery backend" ${LUMACORE_LINUX_DEFAULT})
option(LUMACORE_ENABLE_HIDAPI "Enable read-only hidapi discovery when available" ON)
option(LUMACORE_ENABLE_LIBUSB "Enable read-only libusb discovery when available" ON)
option(LUMACORE_ENABLE_I2C_DEV "Enable optional read-only i2c-dev adapter metadata discovery" OFF)
option(LUMACORE_ENABLE_ASUS_AURA_HID "Build ASUS Aura USB HID backend with config-verified confirmation-gated writes" ${LUMACORE_LINUX_DEFAULT})

set(LUMACORE_HAS_HIDAPI OFF)
set(LUMACORE_HAS_LIBUSB OFF)
set(LUMACORE_HAS_I2C_DEV OFF)

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

qt_add_library(lumacore_core STATIC
    backends/mock/MockBackend.cpp
    backends/mock/MockBackend.h
    backends/mock/MockRgbDevice.cpp
    backends/mock/MockRgbDevice.h
    core/DeviceManager.cpp
    core/DeviceManager.h
    core/BackendRegistry.cpp
    core/BackendRegistry.h
    core/EffectsEngine.cpp
    core/EffectsEngine.h
    core/ActivityLog.cpp
    core/ActivityLog.h
    core/BackendCapabilities.h
    core/PermissionGate.cpp
    core/PermissionGate.h
    core/RgbBackend.cpp
    core/RgbBackend.h
    core/WriteGate.cpp
    core/WriteGate.h
    core/RgbColor.cpp
    core/RgbColor.h
    core/RgbDevice.cpp
    core/RgbDevice.h
    core/RgbEffect.cpp
    core/RgbEffect.h
    core/RgbLed.cpp
    core/RgbLed.h
    core/RgbZone.cpp
    core/RgbZone.h
)

target_include_directories(lumacore_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(lumacore_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
)

target_compile_definitions(lumacore_core
    PUBLIC
        LUMACORE_VERSION="${PROJECT_VERSION}"
)

qt_add_library(lumacore_ipc STATIC
    ipc/DaemonClient.cpp
    ipc/DaemonClient.h
    ipc/DaemonProtocol.cpp
    ipc/DaemonProtocol.h
    ipc/DaemonServer.cpp
    ipc/DaemonServer.h
)

target_include_directories(lumacore_ipc
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(lumacore_ipc
    PUBLIC
        lumacore_core
        Qt6::Network
)

qt_add_library(lumacore_daemon_backend STATIC
    backends/daemon/DaemonBackend.cpp
    backends/daemon/DaemonBackend.h
    backends/daemon/DaemonRgbDevice.cpp
    backends/daemon/DaemonRgbDevice.h
)

target_include_directories(lumacore_daemon_backend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(lumacore_daemon_backend
    PUBLIC
        lumacore_core
        lumacore_ipc
)

if(LUMACORE_ENABLE_LINUX_DISCOVERY AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    qt_add_library(lumacore_linux_backend STATIC
        backends/linux/LinuxDiscoveryBackend.cpp
        backends/linux/LinuxDiscoveryBackend.h
        backends/linux/LinuxDiscoveredDevice.cpp
        backends/linux/LinuxDiscoveredDevice.h
        hardware/linux/HidapiProbe.cpp
        hardware/linux/HidapiProbe.h
        hardware/linux/I2cDevProbe.cpp
        hardware/linux/I2cDevProbe.h
        hardware/linux/LibusbProbe.cpp
        hardware/linux/LibusbProbe.h
        hardware/linux/ProbeResult.cpp
        hardware/linux/ProbeResult.h
    )

    target_include_directories(lumacore_linux_backend
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(lumacore_linux_backend
        PUBLIC
            lumacore_core
    )

    if(LUMACORE_HAS_HIDAPI)
        target_compile_definitions(lumacore_linux_backend PRIVATE LUMACORE_HAS_HIDAPI)
        target_link_libraries(lumacore_linux_backend PRIVATE ${LUMACORE_HIDAPI_TARGET})
    endif()

    if(LUMACORE_HAS_LIBUSB)
        target_compile_definitions(lumacore_linux_backend PRIVATE LUMACORE_HAS_LIBUSB)
        target_link_libraries(lumacore_linux_backend PRIVATE ${LUMACORE_LIBUSB_TARGET})
    endif()

    if(LUMACORE_HAS_I2C_DEV)
        target_compile_definitions(lumacore_linux_backend PRIVATE LUMACORE_HAS_I2C_DEV)
    endif()
endif()

if(LUMACORE_ENABLE_ASUS_AURA_HID AND CMAKE_SYSTEM_NAME STREQUAL "Linux" AND LUMACORE_HAS_HIDAPI AND TARGET lumacore_linux_backend)
    qt_add_library(lumacore_asus_aura_backend STATIC
        backends/asus/AsusAuraHidBackend.cpp
        backends/asus/AsusAuraHidBackend.h
        backends/asus/AsusAuraHidDevice.cpp
        backends/asus/AsusAuraHidDevice.h
        hardware/linux/AsusAuraHidProtocol.cpp
        hardware/linux/AsusAuraHidProtocol.h
        hardware/linux/HidDeviceWriter.cpp
        hardware/linux/HidDeviceWriter.h
    )

    target_include_directories(lumacore_asus_aura_backend
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_compile_definitions(lumacore_asus_aura_backend PRIVATE LUMACORE_HAS_HIDAPI)
    target_link_libraries(lumacore_asus_aura_backend
        PUBLIC
            lumacore_core
            lumacore_linux_backend
        PRIVATE
            ${LUMACORE_HIDAPI_TARGET}
    )

    qt_add_executable(lumacore_asus_aura_protocol_test
        tests/AsusAuraHidProtocolTest.cpp
        hardware/linux/AsusAuraHidProtocol.cpp
        hardware/linux/AsusAuraHidProtocol.h
    )

    target_include_directories(lumacore_asus_aura_protocol_test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(lumacore_asus_aura_protocol_test
        PRIVATE
            lumacore_core
            Qt6::Core
    )

    add_test(NAME asus_aura_hid_protocol COMMAND lumacore_asus_aura_protocol_test)
endif()

enable_testing()

qt_add_executable(lumacore_device_manager_write_gate_test
    tests/DeviceManagerWriteGateTest.cpp
)

target_include_directories(lumacore_device_manager_write_gate_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(lumacore_device_manager_write_gate_test
    PRIVATE
        lumacore_core
        Qt6::Core
)

add_test(NAME device_manager_write_gate COMMAND lumacore_device_manager_write_gate_test)

qt_add_executable(lumacore_daemon_protocol_test
    tests/DaemonProtocolTest.cpp
)

target_include_directories(lumacore_daemon_protocol_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(lumacore_daemon_protocol_test
    PRIVATE
        lumacore_core
        lumacore_ipc
        Qt6::Core
)

add_test(NAME daemon_protocol COMMAND lumacore_daemon_protocol_test)

qt_add_executable(lumacore
    MANUAL_FINALIZATION
    app/main.cpp
    ui/AppController.cpp
    ui/AppController.h
    ui/DeviceListModel.cpp
    ui/DeviceListModel.h
    ui/DeviceTreeModel.cpp
    ui/DeviceTreeModel.h
    ui/SettingsController.cpp
    ui/SettingsController.h
    ui/ZoneListModel.cpp
    ui/ZoneListModel.h
)

qt_add_executable(lumacore-daemon
    MANUAL_FINALIZATION
    daemon/main.cpp
)

qt_add_resources(lumacore lumacore_icons
    PREFIX "/icons"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons"
    FILES
        assets/icons/lumacore.ico
        assets/icons/lumacore-16.png
        assets/icons/lumacore-32.png
        assets/icons/lumacore-48.png
        assets/icons/lumacore-64.png
        assets/icons/lumacore-128.png
        assets/icons/lumacore-256.png
)

set_source_files_properties(ui/qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(lumacore
    URI LumaCore
    VERSION 1.0
    QML_FILES
        ui/qml/AboutDialog.qml
        ui/qml/Theme.qml
        ui/qml/components/AppButton.qml
        ui/qml/components/AppSlider.qml
        ui/qml/components/AppSwitch.qml
        ui/qml/components/ActivityLogPanel.qml
        ui/qml/components/BackendInfoDialog.qml
        ui/qml/components/BackendStatusChip.qml
        ui/qml/components/ColorPickerDialog.qml
        ui/qml/components/DeviceTreePanel.qml
        ui/qml/components/EffectSelector.qml
        ui/qml/components/NavIcon.qml
        ui/qml/components/NavItem.qml
        ui/qml/components/NavRail.qml
        ui/qml/components/PillLabel.qml
        ui/qml/components/ProfileManager.qml
        ui/qml/components/SectionCard.qml
        ui/qml/components/SettingsPage.qml
        ui/qml/components/ZoneEditor.qml
        ui/qml/Main.qml
)

target_include_directories(lumacore
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(lumacore
    PRIVATE
        lumacore_core
        lumacore_daemon_backend
        lumacore_ipc
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickDialogs2
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

message(STATUS "LumaCore Linux discovery: ${LUMACORE_ENABLE_LINUX_DISCOVERY}")
message(STATUS "LumaCore hidapi provider: ${LUMACORE_HAS_HIDAPI}")
message(STATUS "LumaCore libusb provider: ${LUMACORE_HAS_LIBUSB}")
message(STATUS "LumaCore i2c-dev provider: ${LUMACORE_HAS_I2C_DEV}")
message(STATUS "LumaCore ASUS Aura HID backend: ${LUMACORE_ENABLE_ASUS_AURA_HID}")

qt_finalize_executable(lumacore)
qt_finalize_executable(lumacore-daemon)
