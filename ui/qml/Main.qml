import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import LumaCore
import "components"

ApplicationWindow {
    id: root

    width: 1180
    height: 760
    minimumWidth: 920
    minimumHeight: 620
    visible: true
    title: qsTr("LumaCore")
    color: Theme.window

    property color selectedColor: "#4080FF"
    property bool sidebarCollapsed: false
    property int currentPage: 0
    property int selectedDeviceIndex: -1
    property int selectedZoneIndex: -1
    property real sidebarWidth: sidebarCollapsed ? 74 : 248
    readonly property var controller: appController
    readonly property var settings: settingsController
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
        { "title": qsTr("Profiles"), "subtitle": qsTr("Save and restore device state while the backend is mock-only.") },
        { "title": qsTr("Settings"), "subtitle": qsTr("Visual preferences and startup behavior for the app shell.") }
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
        zoneModel.deviceIndex = deviceIndex
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
        zoneModel.deviceIndex = deviceIndex
        selectedColor = controller.zoneEffectColor(deviceIndex, zoneIndex)
    }

    Behavior on sidebarWidth {
        NumberAnimation {
            duration: root.animationDuration
            easing.type: Easing.OutCubic
        }
    }

    ColorDialog {
        id: colorDialog

        title: qsTr("Choose Color")
        selectedColor: root.selectedColor
        onAccepted: root.selectedColor = selectedColor
    }

    AboutDialog {
        id: aboutDialog
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
                                color: Theme.success
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

                    PillLabel {
                        Layout.alignment: Qt.AlignVCenter
                        text: qsTr("Mock backend")
                        animationsEnabled: root.animationsEnabled
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
                            title: qsTr("Device Tree")
                            subtitle: qsTr("Pick a device or zone")
                            animationsEnabled: root.animationsEnabled

                            DeviceTreePanel {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                treeModel: deviceTreeModel
                                selectedDeviceIndex: root.selectedDeviceIndex
                                selectedZoneIndex: root.selectedZoneIndex
                                animationsEnabled: root.animationsEnabled
                                onDeviceSelected: function(deviceIndex) { root.selectDevice(deviceIndex) }
                                onZoneSelected: function(deviceIndex, zoneIndex) { root.selectZone(deviceIndex, zoneIndex) }
                            }
                        }

                        SectionCard {
                            Layout.preferredWidth: 380
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

                        SectionCard {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            title: qsTr("Activity")
                            subtitle: qsTr("Recent mock backend and profile activity")
                            animationsEnabled: root.animationsEnabled

                            TextArea {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                text: root.controller.logText
                                readOnly: true
                                wrapMode: TextArea.Wrap
                                color: Theme.primaryText
                                selectedTextColor: Theme.primaryText
                                selectionColor: Theme.accent
                                font.family: "monospace"
                                font.pixelSize: 12

                                background: Rectangle {
                                    color: Theme.inputBg
                                    radius: 12
                                    border.color: Theme.border
                                }
                            }
                        }
                    }
                }

                Item {
                    SectionCard {
                        anchors.fill: parent
                        title: qsTr("Profile Manager")
                        subtitle: qsTr("Save and restore device state while the backend is still mock-only.")
                        animationsEnabled: root.animationsEnabled

                        ProfileManager {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            appController: root.controller
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
                            animationsEnabled: root.animationsEnabled
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
                // Only follow the zone's color for static effects; animated effects
                // manage their own base color and would otherwise fight the picker.
                if (root.controller.zoneEffectType(deviceIndex, zoneIndex) === 0) {
                    root.selectedColor = root.controller.zoneEffectColor(deviceIndex, zoneIndex)
                }
            }
        }
    }

    Component.onCompleted: root.selectDevice(0)
}
