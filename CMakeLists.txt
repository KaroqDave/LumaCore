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

add_subdirectory(app)

set_source_files_properties(ui/qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(lumacore
    URI LumaCore
    VERSION 1.0
    NO_GENERATE_QMLTYPES
    QML_FILES
        ui/qml/AboutDialog.qml
        ui/qml/Theme.qml
        ui/qml/components/ActivityLogPanel.qml
        ui/qml/components/AppButton.qml
        ui/qml/components/AppSlider.qml
        ui/qml/components/AppSwitch.qml
        ui/qml/components/BackendInfoDialog.qml
        ui/qml/components/BackendStatusChip.qml
        ui/qml/components/ColorPickerDialog.qml
        ui/qml/components/DeviceTreePanel.qml
        ui/qml/components/EffectSelector.qml
        ui/qml/components/NavIcon.qml
        ui/qml/components/NavItem.qml
        ui/qml/components/NavRail.qml
        ui/qml/components/ProfileManager.qml
        ui/qml/components/SectionCard.qml
        ui/qml/components/SettingsPage.qml
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
