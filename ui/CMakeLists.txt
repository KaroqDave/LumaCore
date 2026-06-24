qt_add_library(lumacore_ui STATIC
    AppController.cpp
    AppController.h
    DeviceGroupStore.cpp
    DeviceGroupStore.h
    DeviceTreeModel.cpp
    DeviceTreeModel.h
    SettingsController.cpp
    SettingsController.h
    ZoneEffectPreferenceStore.cpp
    ZoneEffectPreferenceStore.h
)

target_include_directories(lumacore_ui
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(lumacore_ui
    PUBLIC
        lumacore_core
        lumacore_ipc
        Qt6::Core
        Qt6::Gui
    PRIVATE
        lumacore_daemon_backend
)
