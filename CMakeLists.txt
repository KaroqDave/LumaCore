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

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

qt_add_executable(lumacore
    MANUAL_FINALIZATION
    app/main.cpp
    core/DeviceManager.cpp
    core/DeviceManager.h
    core/RgbColor.cpp
    core/RgbColor.h
    core/RgbDevice.cpp
    core/RgbDevice.h
    core/RgbLed.cpp
    core/RgbLed.h
    core/RgbZone.cpp
    core/RgbZone.h
    backends/mock/MockRgbDevice.cpp
    backends/mock/MockRgbDevice.h
    ui/DeviceListModel.cpp
    ui/DeviceListModel.h
)

qt_add_qml_module(lumacore
    URI LumaCore
    VERSION 1.0
    QML_FILES
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
)

qt_finalize_executable(lumacore)
