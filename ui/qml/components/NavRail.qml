import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LumaCore

Rectangle {
    id: rail

    property bool collapsed: false
    property int currentIndex: 0
    property bool animationsEnabled: true
    readonly property int animationDuration: animationsEnabled ? 200 : 0

    property var navModel: [
        { "iconName": "devices", "label": qsTr("Devices"), "badge": "" },
        { "iconName": "profiles", "label": qsTr("Profiles"), "badge": "" },
        { "iconName": "settings", "label": qsTr("Settings"), "badge": "" },
        { "iconName": "activity", "label": qsTr("Activities"), "badge": "" }
    ]

    signal toggleRequested()
    signal navSelected(int index)
    signal aboutRequested()

    color: Theme.surface
    radius: 18
    border.color: Theme.border
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: rail.collapsed ? 12 : 14
        spacing: 14

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: rail.collapsed ? (logo.height + 12 + toggle.height) : logo.height

            Behavior on Layout.preferredHeight {
                NumberAnimation {
                    duration: rail.animationDuration
                    easing.type: Easing.OutCubic
                }
            }

            Rectangle {
                id: logo

                width: 46
                height: 46
                radius: 14
                color: Theme.elevated
                border.color: Theme.accent
                border.width: 1
                y: 0
                x: rail.collapsed ? (parent.width - width) / 2 : 0

                Behavior on x {
                    NumberAnimation {
                        duration: rail.animationDuration
                        easing.type: Easing.OutCubic
                    }
                }

                Image {
                    id: logoImage

                    anchors.centerIn: parent
                    width: 36
                    height: 36
                    source: "qrc:///icons/lumacore-256.png"
                    sourceSize.width: 72
                    sourceSize.height: 72
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    visible: status === Image.Ready
                }

                Label {
                    anchors.centerIn: parent
                    visible: logoImage.status !== Image.Ready
                    text: qsTr("LC")
                    color: Theme.primaryText
                    font.pixelSize: 17
                    font.bold: true
                }
            }

            Rectangle {
                id: toggle

                width: 34
                height: 34
                radius: 17
                color: toggleArea.containsMouse ? Theme.hover : Theme.elevated
                border.color: toggleArea.containsMouse ? Theme.accent : Theme.border
                border.width: 1
                x: rail.collapsed ? (parent.width - width) / 2 : (parent.width - width)
                y: rail.collapsed ? (logo.height + 12) : (logo.height - height) / 2

                Behavior on x {
                    NumberAnimation {
                        duration: rail.animationDuration
                        easing.type: Easing.OutCubic
                    }
                }

                Behavior on y {
                    NumberAnimation {
                        duration: rail.animationDuration
                        easing.type: Easing.OutCubic
                    }
                }

                Behavior on color {
                    ColorAnimation {
                        duration: rail.animationsEnabled ? 140 : 0
                    }
                }

                NavIcon {
                    anchors.centerIn: parent
                    width: 18
                    height: 18
                    name: "chevron"
                    color: Theme.primaryText
                    strokeWidth: 2
                    rotation: rail.collapsed ? 180 : 0

                    Behavior on rotation {
                        NumberAnimation {
                            duration: rail.animationDuration
                            easing.type: Easing.OutCubic
                        }
                    }
                }

                MouseArea {
                    id: toggleArea

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: rail.toggleRequested()
                }

                ToolTip.visible: toggleArea.containsMouse
                ToolTip.text: rail.collapsed ? qsTr("Expand sidebar") : qsTr("Collapse sidebar")
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            Layout.topMargin: 2
            color: Theme.divider
            opacity: 0.7
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: rail.navModel

                NavItem {
                    Layout.fillWidth: true
                    iconName: modelData.iconName
                    label: modelData.label
                    badgeText: modelData.badge
                    selected: rail.currentIndex === index
                    collapsed: rail.collapsed
                    animationsEnabled: rail.animationsEnabled
                    onClicked: rail.navSelected(index)
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }

        NavItem {
            Layout.fillWidth: true
            iconName: "info"
            label: qsTr("About")
            collapsed: rail.collapsed
            animationsEnabled: rail.animationsEnabled
            onClicked: rail.aboutRequested()
        }
    }
}
