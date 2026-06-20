qt_add_executable(lumacore
    MANUAL_FINALIZATION
    DaemonLauncher.cpp
    DaemonLauncher.h
    GuiApplication.cpp
    GuiApplication.h
    GuiOptions.cpp
    GuiOptions.h
    QmlHost.cpp
    QmlHost.h
    VrrFlickerGuard.cpp
    VrrFlickerGuard.h
    main.cpp
)

if(WIN32)
    set_target_properties(lumacore PROPERTIES WIN32_EXECUTABLE TRUE)

    file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}/assets/icons/lumacore.ico" LUMACORE_APP_ICON_ICO)
    string(REPLACE "\\" "/" LUMACORE_APP_ICON_ICO "${LUMACORE_APP_ICON_ICO}")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/lumacore.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/lumacore.rc"
        @ONLY
    )
    target_sources(lumacore PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/lumacore.rc")
    set_source_files_properties(
        "${CMAKE_CURRENT_BINARY_DIR}/lumacore.rc"
        PROPERTIES OBJECT_DEPENDS "${PROJECT_SOURCE_DIR}/assets/icons/lumacore.ico"
    )
endif()

qt_add_resources(lumacore lumacore_icons
    PREFIX "/icons"
    BASE "${PROJECT_SOURCE_DIR}/assets/icons"
    FILES
        ${PROJECT_SOURCE_DIR}/assets/icons/lumacore-16.png
        ${PROJECT_SOURCE_DIR}/assets/icons/lumacore-32.png
        ${PROJECT_SOURCE_DIR}/assets/icons/lumacore-48.png
        ${PROJECT_SOURCE_DIR}/assets/icons/lumacore-64.png
        ${PROJECT_SOURCE_DIR}/assets/icons/lumacore-128.png
        ${PROJECT_SOURCE_DIR}/assets/icons/lumacore-256.png
)

target_include_directories(lumacore
    PRIVATE
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(lumacore
    PRIVATE
        lumacore_core
        lumacore_daemon_backend
        lumacore_ipc
        lumacore_ui
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickDialogs2
)
