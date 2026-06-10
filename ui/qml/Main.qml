import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "components"

ApplicationWindow {
    id: root

    width: 1180
    height: 760
    minimumWidth: 920
    minimumHeight: 620
    visible: true
    title: qsTr("LumaCore")
    color: "#15191D"

    property color selectedColor: "#4080FF"
    property bool sidebarCollapsed: false
    property int selectedDeviceIndex: -1
    property int selectedZoneIndex: -1
    property real sidebarWidth: sidebarCollapsed ? 74 : 300
    readonly property var controller: appController
    readonly property var settings: settingsController
    readonly property bool animationsEnabled: settings.animationsEnabled
    readonly property int animationDuration: animationsEnabled ? 180 : 0
    readonly property color surfaceColor: "#1E242A"
    readonly property color elevatedColor: "#252B32"
    readonly property color accentColor: "#42A5F5"
    readonly property color borderColor: "#343C44"
    readonly property color primaryTextColor: "#F2F5F8"
    readonly property color secondaryTextColor: "#AEB8C2"

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
            selectedColor = controller.zoneColor(selectedDeviceIndex, selectedZoneIndex)
        }
    }

    function selectZone(deviceIndex, zoneIndex) {
        if (deviceIndex < 0 || zoneIndex < 0) {
            return
        }

        selectedDeviceIndex = deviceIndex
        selectedZoneIndex = zoneIndex
        zoneModel.deviceIndex = deviceIndex
        selectedColor = controller.zoneColor(deviceIndex, zoneIndex)
    }

    Behavior on sidebarWidth {
        NumberAnimation {
            duration: root.animationDuration
            easing.type: Easing.OutCubic
        }
    }

    ColorDialog {
        id: colorDialog

        title: qsTr("Choose Static Color")
        selectedColor: root.selectedColor
        onAccepted: root.selectedColor = selectedColor
    }

    AboutDialog {
        id: aboutDialog
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 62
            radius: 18
            color: root.surfaceColor
            border.color: root.borderColor
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 42
                    Layout.preferredHeight: 42
                    radius: 13
                    color: root.elevatedColor
                    border.color: root.accentColor
                    border.width: 1

                    Image {
                        id: headerIconImage

                        anchors.centerIn: parent
                        width: 34
                        height: 34
                        source: "qrc:///icons/lumacore-256.png"
                        sourceSize.width: 68
                        sourceSize.height: 68
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        visible: status === Image.Ready
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: headerIconImage.status !== Image.Ready
                        text: qsTr("LC")
                        color: root.primaryTextColor
                        font.pixelSize: 16
                        font.bold: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("LumaCore")
                        color: root.primaryTextColor
                        font.pixelSize: 23
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Mock-safe RGB control with compact device, zone, profile, and settings tools.")
                        color: root.secondaryTextColor
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                PillLabel {
                    text: qsTr("Mock backend")
                    animationsEnabled: root.animationsEnabled
                }

                ToolButton {
                    id: aboutButton

                    Layout.preferredWidth: 38
                    Layout.preferredHeight: 38
                    icon.name: "dialog-information"
                    icon.width: 20
                    icon.height: 20
                    ToolTip.text: qsTr("About LumaCore")
                    ToolTip.visible: hovered
                    onClicked: aboutDialog.open()

                    background: Rectangle {
                        radius: 12
                        color: aboutButton.down ? "#2A3540" : (aboutButton.hovered ? "#24313B" : root.elevatedColor)
                        border.color: aboutButton.hovered ? root.accentColor : root.borderColor
                        border.width: 1

                        Behavior on color {
                            ColorAnimation {
                                duration: root.animationDuration
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            color: root.elevatedColor
            radius: 14
            border.color: root.borderColor

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Rectangle {
                    Layout.preferredWidth: 9
                    Layout.preferredHeight: 9
                    radius: 5
                    color: "#4CAF50"
                }

                Label {
                    Layout.fillWidth: true
                    text: root.controller.statusMessage
                    color: root.primaryTextColor
                    font.pixelSize: 13
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

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            Sidebar {
                id: sidebar

                Layout.preferredWidth: root.sidebarWidth
                Layout.minimumWidth: root.sidebarCollapsed ? 74 : 260
                Layout.maximumWidth: root.sidebarCollapsed ? 74 : 340
                Layout.fillHeight: true
                collapsed: root.sidebarCollapsed
                treeModel: deviceTreeModel
                selectedDeviceIndex: root.selectedDeviceIndex
                selectedZoneIndex: root.selectedZoneIndex
                surfaceColor: root.surfaceColor
                elevatedColor: root.elevatedColor
                accentColor: root.accentColor
                borderColor: root.borderColor
                primaryTextColor: root.primaryTextColor
                secondaryTextColor: root.secondaryTextColor
                animationsEnabled: root.animationsEnabled
                onToggleRequested: root.sidebarCollapsed = !root.sidebarCollapsed
                onDeviceSelected: function(deviceIndex) { root.selectDevice(deviceIndex) }
                onZoneSelected: function(deviceIndex, zoneIndex) { root.selectZone(deviceIndex, zoneIndex) }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                surfaceColor: root.surfaceColor
                borderColor: root.borderColor
                primaryTextColor: root.primaryTextColor
                secondaryTextColor: root.secondaryTextColor
                animationsEnabled: root.animationsEnabled

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12

                    TabBar {
                        id: mainTabs

                        Layout.fillWidth: true

                        TabButton {
                            text: qsTr("Control")
                        }

                        TabButton {
                            text: qsTr("Profiles")
                        }

                        TabButton {
                            text: qsTr("Settings")
                        }
                    }

                    StackLayout {
                        id: pages

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: mainTabs.currentIndex

                        Item {
                            RowLayout {
                                anchors.fill: parent
                                spacing: 12

                                SectionCard {
                                    Layout.preferredWidth: 390
                                    Layout.minimumWidth: 330
                                    Layout.fillHeight: true
                                    title: qsTr("Zone Editor")
                                    subtitle: qsTr("Rename zones, tune LED counts, and apply static color.")
                                    surfaceColor: root.elevatedColor
                                    borderColor: root.borderColor
                                    primaryTextColor: root.primaryTextColor
                                    secondaryTextColor: root.secondaryTextColor
                                    animationsEnabled: root.animationsEnabled

                                    ZoneEditor {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        appController: root.controller
                                        selectedDeviceIndex: root.selectedDeviceIndex
                                        selectedZoneIndex: root.selectedZoneIndex
                                        selectedColor: root.selectedColor
                                        elevatedColor: root.surfaceColor
                                        accentColor: root.accentColor
                                        borderColor: root.borderColor
                                        primaryTextColor: root.primaryTextColor
                                        secondaryTextColor: root.secondaryTextColor
                                        animationsEnabled: root.animationsEnabled
                                        onChooseColorRequested: colorDialog.open()
                                    }
                                }

                                SectionCard {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    title: qsTr("Activity")
                                    subtitle: qsTr("Recent mock backend and profile activity")
                                    surfaceColor: root.elevatedColor
                                    borderColor: root.borderColor
                                    primaryTextColor: root.primaryTextColor
                                    secondaryTextColor: root.secondaryTextColor
                                    animationsEnabled: root.animationsEnabled

                                    TextArea {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        text: root.controller.logText
                                        readOnly: true
                                        wrapMode: TextArea.Wrap
                                        color: root.primaryTextColor
                                        selectedTextColor: root.primaryTextColor
                                        selectionColor: root.accentColor
                                        font.family: "monospace"
                                        font.pixelSize: 12

                                        background: Rectangle {
                                            color: "#171C21"
                                            radius: 12
                                            border.color: root.borderColor
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
                                surfaceColor: root.elevatedColor
                                borderColor: root.borderColor
                                primaryTextColor: root.primaryTextColor
                                secondaryTextColor: root.secondaryTextColor
                                animationsEnabled: root.animationsEnabled

                                ProfileManager {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    appController: root.controller
                                    borderColor: root.borderColor
                                    primaryTextColor: root.primaryTextColor
                                    secondaryTextColor: root.secondaryTextColor
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
                                    elevatedColor: root.elevatedColor
                                    borderColor: root.borderColor
                                    primaryTextColor: root.primaryTextColor
                                    secondaryTextColor: root.secondaryTextColor
                                    animationsEnabled: root.animationsEnabled
                                }
                            }
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
                root.selectedColor = root.controller.zoneColor(deviceIndex, zoneIndex)
            }
        }
    }

    Component.onCompleted: root.selectDevice(0)
}
