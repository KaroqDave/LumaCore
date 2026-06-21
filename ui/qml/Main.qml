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

    width: 1180
    height: 760
    minimumWidth: 920
    minimumHeight: 620
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
    property real sidebarWidth: sidebarCollapsed ? 74 : 248
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

    function colorToHex(value) {
        const red = Math.round(value.r * 255).toString(16).padStart(2, "0")
        const green = Math.round(value.g * 255).toString(16).padStart(2, "0")
        const blue = Math.round(value.b * 255).toString(16).padStart(2, "0")
        return ("#" + red + green + blue).toUpperCase()
    }

    function selectDevice(deviceIndex) {
        if (deviceIndex < 0) {
            return
        }

        selectedDeviceIndex = deviceIndex
        selectedZoneIndex = controller.zoneCount(deviceIndex) > 0 ? 0 : -1
        if (selectedZoneIndex >= 0) {
            selectedColor = controller.zoneEffectColor(selectedDeviceIndex, selectedZoneIndex)
        }
    }

    function selectZone(deviceIndex, zoneIndex) {
        if (deviceIndex < 0 || zoneIndex < 0) {
            return
        }

        selectedDeviceIndex = deviceIndex
        selectedZoneIndex = zoneIndex
        selectedColor = controller.zoneEffectColor(deviceIndex, zoneIndex)
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

    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 14

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
            spacing: 14

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                radius: 18
                color: Theme.surface
                border.color: Theme.border
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 16
                    anchors.topMargin: 12
                    anchors.bottomMargin: 12
                    spacing: 14

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: root.pageTitles[root.currentPage].title
                            color: Theme.primaryText
                            font.pixelSize: 22
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
                        implicitWidth: statusRow.implicitWidth + 24
                        implicitHeight: 34
                        radius: 999
                        color: Theme.elevated
                        border.color: Theme.border
                        border.width: 1

                        RowLayout {
                            id: statusRow

                            anchors.centerIn: parent
                            spacing: 8

                            Rectangle {
                                Layout.preferredWidth: 9
                                Layout.preferredHeight: 9
                                radius: 5
                                color: root.controller && root.controller.daemonConnected ? Theme.success : Theme.warning
                            }

                            Label {
                                Layout.maximumWidth: 320
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
                Layout.preferredHeight: visible ? 48 : 0
                radius: 14
                color: Theme.warningBg
                border.color: Theme.warning
                border.width: 1

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
                        text: qsTr("Windows Preview: Mock devices only—hardware discovery and RGB writes are not supported.")
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
                        spacing: 14

                        SectionCard {
                            Layout.preferredWidth: 280
                            Layout.minimumWidth: 240
                            Layout.fillHeight: true
                            animationsEnabled: root.animationsEnabled

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
                            }
                        }

                        SectionCard {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 320
                            Layout.fillHeight: true
                            title: qsTr("Zone Editor")
                            subtitle: qsTr("Rename zones, tune LED counts, and apply lighting effects.")
                            animationsEnabled: root.animationsEnabled

                            ScrollView {
                                id: zoneScroll

                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                contentWidth: availableWidth

                                ZoneEditor {
                                    width: zoneScroll.availableWidth
                                    height: Math.max(implicitHeight, zoneScroll.availableHeight)
                                    appController: root.controller
                                    selectedDeviceIndex: root.selectedDeviceIndex
                                    selectedZoneIndex: root.selectedZoneIndex
                                    selectedColor: root.selectedColor
                                    animationsEnabled: root.animationsEnabled
                                    onChooseColorRequested: colorDialog.open()
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
                // Static and Breathing use a base color. Rainbow and Color Cycle
                // are generated effects, so following their frame color would fight the picker.
                const effectType = root.controller.zoneEffectType(deviceIndex, zoneIndex)
                if (effectType === 0 || effectType === 2) {
                    root.selectedColor = root.controller.zoneEffectColor(deviceIndex, zoneIndex)
                }
            }
        }
    }

    Component.onCompleted: root.selectDevice(0)
}
