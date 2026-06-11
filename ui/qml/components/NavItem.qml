import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LumaCore

Item {
    id: navItem

    property string iconName: ""
    property string label: ""
    property string badgeText: ""
    property bool selected: false
    property bool collapsed: false
    property bool animationsEnabled: true
    readonly property int animationDuration: animationsEnabled ? 160 : 0

    signal clicked()

    implicitHeight: 46
    Layout.fillWidth: true

    Rectangle {
        id: background

        anchors.fill: parent
        radius: 13
        color: navItem.selected
               ? Theme.selectionBg
               : (hoverArea.containsMouse ? Theme.hover : "transparent")
        border.color: navItem.selected ? Theme.selectionBorder : "transparent"
        border.width: 1

        Behavior on color {
            ColorAnimation {
                duration: navItem.animationDuration
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: navItem.collapsed ? 0 : 14
            anchors.rightMargin: navItem.collapsed ? 0 : 12
            spacing: 12

            Item {
                Layout.preferredWidth: navItem.collapsed ? parent.width : 24
                Layout.preferredHeight: 24
                Layout.alignment: Qt.AlignVCenter

                NavIcon {
                    anchors.centerIn: parent
                    width: 22
                    height: 22
                    name: navItem.iconName
                    color: navItem.selected ? Theme.selectionText : Theme.primaryText
                    strokeWidth: navItem.selected ? 2.2 : 1.8
                }
            }

            Label {
                Layout.fillWidth: true
                visible: !navItem.collapsed
                opacity: navItem.collapsed ? 0 : 1
                text: navItem.label
                color: navItem.selected ? Theme.selectionText : Theme.primaryText
                font.pixelSize: 14
                font.bold: navItem.selected
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter

                Behavior on opacity {
                    NumberAnimation {
                        duration: navItem.animationDuration
                    }
                }
            }

            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                visible: !navItem.collapsed && navItem.badgeText.length > 0
                radius: 8
                color: navItem.selected ? "#FFFFFF" : Theme.accent
                implicitWidth: Math.max(22, badgeLabel.implicitWidth + 12)
                implicitHeight: 22

                Label {
                    id: badgeLabel

                    anchors.centerIn: parent
                    text: navItem.badgeText
                    color: navItem.selected ? Theme.selectionBg : "#FFFFFF"
                    font.pixelSize: 11
                    font.bold: true
                }
            }
        }

        MouseArea {
            id: hoverArea

            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: navItem.clicked()
        }
    }

    ToolTip {
        id: flyout

        parent: navItem
        x: navItem.width + 12
        y: (navItem.height - height) / 2
        visible: navItem.collapsed && hoverArea.containsMouse
        delay: 120
        text: navItem.label

        contentItem: Label {
            text: flyout.text
            color: Theme.primaryText
            font.pixelSize: 13
            font.bold: true
        }

        background: Rectangle {
            color: Theme.elevated
            radius: 10
            border.color: Theme.border
            border.width: 1
        }
    }
}
