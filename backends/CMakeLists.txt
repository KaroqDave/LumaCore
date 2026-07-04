# SPDX-License-Identifier: GPL-2.0-or-later

qt_add_library(lumacore_daemon_backend STATIC
    daemon/DaemonBackend.cpp
    daemon/DaemonBackend.h
    daemon/DaemonRgbDevice.cpp
    daemon/DaemonRgbDevice.h
)

target_include_directories(lumacore_daemon_backend
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(lumacore_daemon_backend
    PUBLIC
        lumacore_core
        lumacore_ipc
)

if(LUMACORE_ENABLE_LINUX_DISCOVERY AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    qt_add_library(lumacore_linux_backend STATIC
        linux/LinuxDiscoveryBackend.cpp
        linux/LinuxDiscoveryBackend.h
        linux/LinuxDiscoveredDevice.cpp
        linux/LinuxDiscoveredDevice.h
        ${PROJECT_SOURCE_DIR}/hardware/linux/HidapiProbe.cpp
        ${PROJECT_SOURCE_DIR}/hardware/linux/HidapiProbe.h
        ${PROJECT_SOURCE_DIR}/hardware/linux/I2cDevProbe.cpp
        ${PROJECT_SOURCE_DIR}/hardware/linux/I2cDevProbe.h
        ${PROJECT_SOURCE_DIR}/hardware/linux/LibusbProbe.cpp
        ${PROJECT_SOURCE_DIR}/hardware/linux/LibusbProbe.h
        ${PROJECT_SOURCE_DIR}/hardware/linux/ProbeResult.cpp
        ${PROJECT_SOURCE_DIR}/hardware/linux/ProbeResult.h
        ${PROJECT_SOURCE_DIR}/hardware/common/ProbeResultCommon.cpp
        ${PROJECT_SOURCE_DIR}/hardware/common/ProbeResultCommon.h
        ${PROJECT_SOURCE_DIR}/hardware/common/ProbeTypes.h
    )

    target_include_directories(lumacore_linux_backend PUBLIC ${PROJECT_SOURCE_DIR})
    target_link_libraries(lumacore_linux_backend PUBLIC lumacore_core)

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

if(LUMACORE_ENABLE_WINDOWS_DISCOVERY AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
    qt_add_library(lumacore_windows_backend STATIC
        windows/WindowsDiscoveryBackend.cpp
        windows/WindowsDiscoveryBackend.h
        windows/WindowsDiscoveredDevice.cpp
        windows/WindowsDiscoveredDevice.h
        ${PROJECT_SOURCE_DIR}/hardware/windows/HidapiProbe.cpp
        ${PROJECT_SOURCE_DIR}/hardware/windows/HidapiProbe.h
        ${PROJECT_SOURCE_DIR}/hardware/windows/ProbeResult.cpp
        ${PROJECT_SOURCE_DIR}/hardware/windows/ProbeResult.h
        ${PROJECT_SOURCE_DIR}/hardware/common/ProbeResultCommon.cpp
        ${PROJECT_SOURCE_DIR}/hardware/common/ProbeResultCommon.h
        ${PROJECT_SOURCE_DIR}/hardware/common/ProbeTypes.h
    )

    target_include_directories(lumacore_windows_backend PUBLIC ${PROJECT_SOURCE_DIR})
    target_link_libraries(lumacore_windows_backend PUBLIC lumacore_core)

    if(LUMACORE_HAS_HIDAPI)
        target_compile_definitions(lumacore_windows_backend PRIVATE LUMACORE_HAS_HIDAPI)
        target_link_libraries(lumacore_windows_backend PRIVATE ${LUMACORE_HIDAPI_TARGET})
    endif()
endif()

if(LUMACORE_ENABLE_ASUS_AURA_HID
   AND LUMACORE_HAS_HIDAPI
   AND ((CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET lumacore_linux_backend)
        OR (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND TARGET lumacore_windows_backend)))
    set(_lumacore_asus_aura_platform_sources)
    set(_lumacore_asus_aura_platform_target)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(_lumacore_asus_aura_platform_sources
            ${PROJECT_SOURCE_DIR}/hardware/linux/HidDeviceWriter.cpp
            ${PROJECT_SOURCE_DIR}/hardware/linux/HidDeviceWriter.h
        )
        set(_lumacore_asus_aura_platform_target lumacore_linux_backend)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(_lumacore_asus_aura_platform_sources
            ${PROJECT_SOURCE_DIR}/hardware/windows/HidDeviceWriter.cpp
            ${PROJECT_SOURCE_DIR}/hardware/windows/HidDeviceWriter.h
        )
        set(_lumacore_asus_aura_platform_target lumacore_windows_backend)
    endif()

    qt_add_library(lumacore_asus_aura_backend STATIC
        asus/AsusAuraHidBackend.cpp
        asus/AsusAuraHidBackend.h
        asus/AsusAuraHidDevice.cpp
        asus/AsusAuraHidDevice.h
        asus/AsusAuraHidPlatform.h
        ${PROJECT_SOURCE_DIR}/hardware/linux/AsusAuraHidProtocol.cpp
        ${PROJECT_SOURCE_DIR}/hardware/linux/AsusAuraHidProtocol.h
        ${_lumacore_asus_aura_platform_sources}
    )

    target_include_directories(lumacore_asus_aura_backend PUBLIC ${PROJECT_SOURCE_DIR})
    target_link_libraries(lumacore_asus_aura_backend
        PUBLIC
            lumacore_core
            ${_lumacore_asus_aura_platform_target}
    )

    if(LUMACORE_HAS_HIDAPI)
        target_compile_definitions(lumacore_asus_aura_backend PRIVATE LUMACORE_HAS_HIDAPI)
        target_link_libraries(lumacore_asus_aura_backend PRIVATE ${LUMACORE_HIDAPI_TARGET})
    endif()
endif()
