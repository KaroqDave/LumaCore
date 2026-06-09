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
    core/RgbColor.cpp
    core/RgbColor.h
    core/RgbDevice.cpp
    core/RgbDevice.h
    core/RgbLed.cpp
    core/RgbLed.h
    core/RgbZone.cpp
    core/RgbZone.h
    ui/AppController.cpp
    ui/AppController.h
    ui/DeviceListModel.cpp
    ui/DeviceListModel.h
    ui/ZoneListModel.cpp
    ui/ZoneListModel.h
)

qt_add_resources(lumacore lumacore_icons
    PREFIX "/icons"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons"
    FILES
        assets/icons/lumacore.svg
        assets/icons/lumacore.ico
        assets/icons/lumacore-16.png
        assets/icons/lumacore-32.png
        assets/icons/lumacore-48.png
        assets/icons/lumacore-64.png
        assets/icons/lumacore-128.png
        assets/icons/lumacore-256.png
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
        Qt6::QuickDialogs2
)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/icons")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/lumacore-256.png"
    "${CMAKE_BINARY_DIR}/icons/lumacore.png"
    COPYONLY
)

set(LUMACORE_BUILD_EXECUTABLE "${CMAKE_BINARY_DIR}/lumacore")
set(LUMACORE_BUILD_ICON "${CMAKE_BINARY_DIR}/icons/lumacore.png")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/lumacore.desktop.in"
    "${CMAKE_BINARY_DIR}/lumacore.desktop"
    @ONLY
)

install(TARGETS lumacore
    RUNTIME DESTINATION bin
)

install(FILES packaging/linux/lumacore.desktop
    DESTINATION share/applications
)

install(FILES assets/icons/lumacore-256.png
    DESTINATION share/pixmaps
    RENAME lumacore.png
)

qt_finalize_executable(lumacore)
