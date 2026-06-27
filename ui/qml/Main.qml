// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import LumaCore
import "components"

ApplicationWindow {
    id: root

    required property var deviceTreeModel
    required property var appController
    required property var settingsController
    required property bool startMinimized

    width: 1360
    height: 840
    minimumWidth: 1260
    minimumHeight: 680
    // Resolve the launch window state declaratively so it is applied during component
    // completion, before the window is first shown, rather than minimizing after the fact.
    visibility: root.startMinimized ? Window.Minimized : Window.Windowed
    title: qsTr("LumaCore")
    color: Theme.window

    property color selectedColor: Theme.defaultColor
    property bool sidebarCollapsed: true
    property int currentPage: 0
    property int selectedDeviceIndex: -1
    property int selectedZoneIndex: -1
    property string selectedDeviceId: ""
    property string selectedZoneName: ""
    property int selectedZoneFallbackIndex: -1
    property real sidebarWidth: sidebarCollapsed ? 66 : 216
    readonly property var controller: root.appController
    readonly property var settings: root.settingsController
    readonly property bool animationsEnabled: settings.animationsEnabled
    readonly property int animationDuration: animationsEnabled ? 180 : 0

    palette.window: Theme.surface
    palette.windowText: Theme.primaryText
    palette.base: Theme.inputBg
    palette.alternateBase: Theme.elevated
    palette.text: Theme.primaryText
    palette.button: Theme.elevated
    palette.buttonText: Theme.primaryText
    palette.highlight: Theme.accent
    palette.highlightedText: "#FFFFFF"
    palette.placeholderText: Theme.secondaryText
    palette.mid: Theme.border
    palette.dark: Theme.border

    readonly property var pageTitles: [
        { "title": qsTr("Devices"), "subtitle": qsTr("Pick a device or zone, then choose a lighting effect.") },
        { "title": qsTr("Profiles"), "subtitle": qsTr("Save and restore device state for the active backend.") },
        { "title": qsTr("Settings"), "subtitle": qsTr("Visual preferences and startup behavior for the app shell.") },
        { "title": qsTr("Activities"), "subtitle": qsTr("Structured activity, warnings, and errors") }
    ]

    Binding {
        target: Theme
        property: "mode"
        value: root.settings.theme
    }

    function selectDevice(deviceIndex) {
        if (deviceIndex < 0 || deviceIndex >= controller.backendDeviceCount) {
            return
        }

        selectedDeviceIndex = deviceIndex
        selectedDeviceId = controller.deviceId(deviceIndex)
        selectedZoneIndex = controller.zoneCount(deviceIndex) > 0 ? 0 : -1
        selectedZoneFallbackIndex = selectedZoneIndex
        selectedZoneName = selectedZoneIndex >= 0
            ? controller.zoneName(deviceIndex, selectedZoneIndex)
            : ""
        if (selectedZoneIndex >= 0) {
            selectedColor = controller.zoneEffectColor(selectedDeviceIndex, selectedZoneIndex)
        }
    }

    function selectZone(deviceIndex, zoneIndex) {
        if (deviceIndex < 0
                || deviceIndex >= controller.backendDeviceCount
                || zoneIndex < 0
                || zoneIndex >= controller.zoneCount(deviceIndex)) {
            return
        }

        selectedDeviceIndex = deviceIndex
        selectedZoneIndex = zoneIndex
        selectedDeviceId = controller.deviceId(deviceIndex)
        selectedZoneName = controller.zoneName(deviceIndex, zoneIndex)
        selectedZoneFallbackIndex = zoneIndex
        selectedColor = controller.zoneEffectColor(deviceIndex, zoneIndex)
    }

    function restoreSelection() {
        let deviceIndex = controller.deviceIndexForId(selectedDeviceId)
        const restoredSameDevice = deviceIndex >= 0
        if (deviceIndex < 0) {
            deviceIndex = controller.backendDeviceCount > 0 ? 0 : -1
        }

        if (deviceIndex < 0) {
            selectedDeviceIndex = -1
            selectedZoneIndex = -1
            selectedDeviceId = ""
            selectedZoneName = ""
            selectedZoneFallbackIndex = -1
            return
        }

        selectedDeviceIndex = deviceIndex
        selectedDeviceId = controller.deviceId(deviceIndex)

        let zoneIndex = restoredSameDevice
            ? controller.zoneIndexForName(deviceIndex, selectedZoneName)
            : -1
        const zoneCount = controller.zoneCount(deviceIndex)
        if (restoredSameDevice
                && zoneIndex < 0
                && selectedZoneFallbackIndex >= 0
                && selectedZoneFallbackIndex < zoneCount) {
            zoneIndex = selectedZoneFallbackIndex
        }
        if (zoneIndex < 0 && zoneCount > 0) {
            zoneIndex = 0
        }

        selectedZoneIndex = zoneIndex
        selectedZoneFallbackIndex = zoneIndex
        selectedZoneName = zoneIndex >= 0 ? controller.zoneName(deviceIndex, zoneIndex) : ""
        if (zoneIndex >= 0) {
            selectedColor = controller.zoneEffectColor(deviceIndex, zoneIndex)
        }
    }

    Behavior on sidebarWidth {
        NumberAnimation {
            duration: root.animationDuration
            easing.type: Easing.OutCubic
        }
    }

    ColorPickerDialog {
        id: colorDialog

        parent: Overlay.overlay
        animationsEnabled: root.animationsEnabled
        selectedColor: root.selectedColor
        onAccepted: root.selectedColor = draftColor
    }

    AboutDialog {
        id: aboutDialog

        parent: Overlay.overlay
        controller: root.controller
        animationsEnabled: root.animationsEnabled
    }

    BackendInfoDialog {
        id: backendInfoDialog

        parent: Overlay.overlay
        controller: root.controller
        animationsEnabled: root.animationsEnabled
    }

    Dialog {
        id: zoneEditorDialog

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(720, parent ? parent.width - 48 : 720)
        height: Math.min(260, parent ? parent.height - 48 : 260)
        modal: true
        title: root.selectedZoneName.length > 0
               ? qsTr("Edit %1").arg(root.selectedZoneName)
               : qsTr("Edit Zone")
        standardButtons: Dialog.NoButton

        contentItem: ZoneEditor {
            appController: root.controller
            selectedDeviceIndex: root.selectedDeviceIndex
            selectedZoneIndex: root.selectedZoneIndex
            animationsEnabled: root.animationsEnabled
        }

        footer: Item {
            implicitHeight: 54

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.bottomMargin: 8
                spacing: 12

                Item {
                    Layout.fillWidth: true
                }

                AppButton {
                    Layout.preferredWidth: 220
                    Layout.preferredHeight: 40
                    variant: "primary"
                    text: qsTr("Close")
                    compact: false
                    controlHeight: 40
                    animationsEnabled: root.animationsEnabled
                    onClicked: zoneEditorDialog.close()
                }

                Item {
                    Layout.fillWidth: true
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        NavRail {
            id: navRail

            Layout.preferredWidth: root.sidebarWidth
            Layout.fillHeight: true
            collapsed: root.sidebarCollapsed
            currentIndex: root.currentPage
            animationsEnabled: root.animationsEnabled
            onToggleRequested: root.sidebarCollapsed = !root.sidebarCollapsed
            onNavSelected: function(index) { root.currentPage = index }
            onAboutRequested: aboutDialog.open()
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 58
                radius: 8
                color: Theme.surface
                border.color: Theme.border
                border.width: 1
                antialiasing: true

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 12
                    anchors.topMargin: 8
                    anchors.bottomMargin: 8
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: root.pageTitles[root.currentPage].title
                            color: Theme.primaryText
                            font.pixelSize: 20
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.pageTitles[root.currentPage].subtitle
                            color: Theme.secondaryText
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }

                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: statusRow.implicitWidth + 22
                        implicitHeight: 28
                        radius: 999
                        color: Theme.subtleSurface
                        border.color: Theme.border
                        border.width: 1

                        RowLayout {
                            id: statusRow

                            anchors.centerIn: parent
                            spacing: 8

                            Rectangle {
                                Layout.preferredWidth: 8
                                Layout.preferredHeight: 8
                                radius: 4
                                color: root.controller && root.controller.daemonRecoveryBusy
                                       ? Theme.accent
                                       : (root.controller && root.controller.daemonConnected
                                          ? Theme.success
                                          : Theme.warning)
                            }

                            Label {
                                Layout.maximumWidth: 300
                                text: root.controller.statusMessage
                                color: Theme.primaryText
                                font.pixelSize: 12
                                font.bold: true
                                elide: Text.ElideRight
                                ToolTip.visible: statusMouse.containsMouse && truncated
                                ToolTip.text: text

                                MouseArea {
                                    id: statusMouse

                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.NoButton
                                }
                            }
                        }
                    }

                    BackendStatusChip {
                        Layout.alignment: Qt.AlignVCenter
                        controller: root.controller
                        animationsEnabled: root.animationsEnabled
                        dryRunEnabled: root.controller ? root.controller.dryRunEnabled : false
                        daemonConnected: root.controller ? root.controller.daemonConnected : false
                        daemonState: root.controller ? root.controller.daemonState : qsTr("Disconnected")
                        setupStatusLevel: root.controller ? root.controller.setupStatusLevel : "warning"
                        setupStatusSummary: root.controller ? root.controller.setupStatusSummary : qsTr("Backend attention required")
                        backendName: root.controller && root.controller.backendDisplayName.length > 0
                                     ? root.controller.backendDisplayName
                                     : qsTr("Daemon backend")
                        onClicked: backendInfoDialog.open()
                    }
                }
            }

            Rectangle {
                visible: Qt.platform.os === "windows"
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? 44 : 0
                radius: 8
                color: Theme.warningBg
                border.color: Theme.warning
                border.width: 1
                opacity: 0.95

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 10

                    NavIcon {
                        name: "info"
                        color: Theme.warning
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Windows Preview: Mock devices only - hardware discovery and RGB writes are not supported.")
                        color: Theme.primaryText
                        font.pixelSize: 12
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }
                }
            }

            StackLayout {
                id: pages

                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.currentPage

                Item {
                    RowLayout {
                        anchors.fill: parent
                        spacing: 10

                        SectionCard {
                            Layout.preferredWidth: root.sidebarCollapsed ? 292 : 306
                            Layout.minimumWidth: 250
                            Layout.fillHeight: true
                            animationsEnabled: root.animationsEnabled
                            compact: true

                            DeviceTreePanel {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                treeModel: root.deviceTreeModel
                                appController: root.controller
                                selectedDeviceIndex: root.selectedDeviceIndex
                                selectedZoneIndex: root.selectedZoneIndex
                                animationsEnabled: root.animationsEnabled
                                onDeviceSelected: function(deviceIndex) { root.selectDevice(deviceIndex) }
                                onZoneSelected: function(deviceIndex, zoneIndex) { root.selectZone(deviceIndex, zoneIndex) }
                                onZoneEditRequested: function(deviceIndex, zoneIndex) {
                                    root.selectZone(deviceIndex, zoneIndex)
                                    zoneEditorDialog.open()
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 320
                            Layout.fillHeight: true
                            spacing: 8

                            SetupStatusPanel {
                                Layout.fillWidth: true
                                controller: root.controller
                                animationsEnabled: root.animationsEnabled
                            }

                            SectionCard {
                                Layout.fillWidth: true
                                animationsEnabled: root.animationsEnabled
                                compact: true

                                GlobalControls {
                                    id: globalControls

                                    Layout.fillWidth: true
                                    appController: root.controller
                                    selectedDeviceIndex: root.selectedDeviceIndex
                                    selectedZoneIndex: root.selectedZoneIndex
                                    selectedDeviceId: root.selectedDeviceId
                                    selectedZoneName: root.selectedZoneName
                                    selectedColor: root.selectedColor
                                    animationsEnabled: root.animationsEnabled
                                    onChooseColorRequested: colorDialog.open()
                                    onSelectedColorSyncRequested: function(colorValue) { root.selectedColor = colorValue }
                                }
                            }

                            SectionCard {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                title: qsTr("Workspace")
                                animationsEnabled: root.animationsEnabled
                                compact: true

                                DeviceWorkspacePanel {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    appController: root.controller
                                    selectedDeviceIndex: root.selectedDeviceIndex
                                    selectedZoneIndex: root.selectedZoneIndex
                                    selectedZoneName: root.selectedZoneName
                                    selectedZoneMode: globalControls.selectedZoneMode
                                    selectedTargetName: globalControls.selectedTargetName
                                    animationsEnabled: root.animationsEnabled
                                    onPresetApplied: function(effectType, colorValue, speedValue, brightnessValue) {
                                        root.selectedColor = colorValue
                                        globalControls.syncDraftEffect(effectType, colorValue, speedValue, brightnessValue)
                                    }
                                }
                            }
                        }
                    }
                }

                Item {
                    SectionCard {
                        anchors.fill: parent
                        title: qsTr("Profile Manager")
                        subtitle: qsTr("Save and restore device state for the active backend.")
                        animationsEnabled: root.animationsEnabled
                        compact: true

                        ProfileManager {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            appController: root.controller
                            settingsController: root.settings
                            animationsEnabled: root.animationsEnabled
                        }
                    }
                }

                Item {
                    ScrollView {
                        id: settingsScroll

                        anchors.fill: parent
                        clip: true
                        contentWidth: availableWidth

                        SettingsPage {
                            width: settingsScroll.availableWidth
                            height: Math.max(implicitHeight, settingsScroll.availableHeight)
                            settingsController: root.settings
                            appController: root.controller
                            animationsEnabled: root.animationsEnabled
                        }
                    }
                }

                Item {
                    SectionCard {
                        anchors.fill: parent
                        title: qsTr("Activity Log")
                        subtitle: qsTr("Structured activity, warnings, and errors")
                        animationsEnabled: root.animationsEnabled
                        compact: true

                        ActivityLogPanel {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            controller: root.controller
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: root.controller

        function onZoneDataChanged(deviceIndex, zoneIndex) {
            if (deviceIndex === root.selectedDeviceIndex && zoneIndex === root.selectedZoneIndex) {
                root.selectedDeviceId = root.controller.deviceId(deviceIndex)
                root.selectedZoneName = root.controller.zoneName(deviceIndex, zoneIndex)
                root.selectedZoneFallbackIndex = zoneIndex
                // Static and Breathing use a base color. Rainbow and Color Cycle
                // are generated effects, so following their frame color would fight the picker.
                const effectType = root.controller.zoneEffectType(deviceIndex, zoneIndex)
                if (effectType === 0 || effectType === 2) {
                    root.selectedColor = root.controller.zoneEffectColor(deviceIndex, zoneIndex)
                }
            }
        }

        function onDaemonDevicesRefreshed() {
            root.restoreSelection()
        }
    }

    Component.onCompleted: root.selectDevice(0)
}
