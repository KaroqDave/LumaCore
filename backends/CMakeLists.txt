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
    )

    target_include_directories(lumacore_windows_backend PUBLIC ${PROJECT_SOURCE_DIR})
    target_link_libraries(lumacore_windows_backend PUBLIC lumacore_core)

    if(LUMACORE_HAS_HIDAPI)
        target_compile_definitions(lumacore_windows_backend PRIVATE LUMACORE_HAS_HIDAPI)
        target_link_libraries(lumacore_windows_backend PRIVATE ${LUMACORE_HIDAPI_TARGET})
    endif()
endif()

if(LUMACORE_ENABLE_ASUS_AURA_HID
   AND CMAKE_SYSTEM_NAME STREQUAL "Linux"
   AND LUMACORE_HAS_HIDAPI
   AND TARGET lumacore_linux_backend)
    qt_add_library(lumacore_asus_aura_backend STATIC
        asus/AsusAuraHidBackend.cpp
        asus/AsusAuraHidBackend.h
        asus/AsusAuraHidDevice.cpp
        asus/AsusAuraHidDevice.h
        ${PROJECT_SOURCE_DIR}/hardware/linux/AsusAuraHidProtocol.cpp
        ${PROJECT_SOURCE_DIR}/hardware/linux/AsusAuraHidProtocol.h
        ${PROJECT_SOURCE_DIR}/hardware/linux/HidDeviceWriter.cpp
        ${PROJECT_SOURCE_DIR}/hardware/linux/HidDeviceWriter.h
    )

    target_include_directories(lumacore_asus_aura_backend PUBLIC ${PROJECT_SOURCE_DIR})
    target_compile_definitions(lumacore_asus_aura_backend PRIVATE LUMACORE_HAS_HIDAPI)
    target_link_libraries(lumacore_asus_aura_backend
        PUBLIC
            lumacore_core
            lumacore_linux_backend
        PRIVATE
            ${LUMACORE_HIDAPI_TARGET}
    )
endif()
