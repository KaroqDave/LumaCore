cmake_minimum_required(VERSION 3.24)

project(LumaCore
    VERSION 0.1.0
    DESCRIPTION "Linux-first RGB control application"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 QuickDialogs2)

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

qt_add_executable(lumacore
    MANUAL_FINALIZATION
    app/main.cpp
    backends/mock/MockBackend.cpp
    backends/mock/MockBackend.h
    backends/mock/MockRgbDevice.cpp
    backends/mock/MockRgbDevice.h
    core/DeviceManager.cpp
    core/DeviceManager.h
    core/EffectsEngine.cpp
    core/EffectsEngine.h
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
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickDialogs2
)

target_compile_definitions(lumacore
    PRIVATE
        LUMACORE_VERSION="${PROJECT_VERSION}"
)

qt_finalize_executable(lumacore)
