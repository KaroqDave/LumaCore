# SPDX-License-Identifier: GPL-2.0-or-later

qt_add_library(lumacore_core STATIC
    ${PROJECT_SOURCE_DIR}/backends/mock/MockBackend.cpp
    ${PROJECT_SOURCE_DIR}/backends/mock/MockBackend.h
    ${PROJECT_SOURCE_DIR}/backends/mock/MockRgbDevice.cpp
    ${PROJECT_SOURCE_DIR}/backends/mock/MockRgbDevice.h
    ActivityLog.cpp
    ActivityLog.h
    BackendCapabilities.h
    BackendDiscoveryRunner.cpp
    BackendDiscoveryRunner.h
    BackendRegistry.cpp
    BackendRegistry.h
    DeviceManager.cpp
    DeviceManager.h
    EffectsEngine.cpp
    EffectsEngine.h
    PermissionGate.cpp
    PermissionGate.h
    PortablePaths.cpp
    PortablePaths.h
    ProfilePlan.cpp
    ProfilePlan.h
    ProfileStore.cpp
    ProfileStore.h
    RgbBackend.cpp
    RgbBackend.h
    RgbColor.cpp
    RgbColor.h
    RgbDevice.cpp
    RgbDevice.h
    RgbEffect.cpp
    RgbEffect.h
    RgbLed.cpp
    RgbLed.h
    RgbZone.cpp
    RgbZone.h
    ScheduleService.cpp
    ScheduleService.h
    ScheduleTime.cpp
    ScheduleTime.h
    WriteGate.cpp
    WriteGate.h
)

target_include_directories(lumacore_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}
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
