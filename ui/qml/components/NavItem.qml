import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: navItem

    property string iconName: ""
    property string label: ""
    property string badgeText: ""
    property bool selected: false
    property bool collapsed: false
    property color elevatedColor: "#252B32"
    property color accentColor: "#42A5F5"
    property color borderColor: "#343C44"
    property color primaryTextColor: "#F2F5F8"
    property color secondaryTextColor: "#AEB8C2"
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
               ? "#2D79B8"
               : (hoverArea.containsMouse ? "#26323C" : "transparent")
        border.color: navItem.selected ? "#67C1FF" : "transparent"
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
                    color: navItem.selected ? "#FFFFFF" : navItem.primaryTextColor
                    strokeWidth: navItem.selected ? 2.2 : 1.8
                }
            }

            Label {
                Layout.fillWidth: true
                visible: !navItem.collapsed
                opacity: navItem.collapsed ? 0 : 1
                text: navItem.label
                color: navItem.selected ? "#FFFFFF" : navItem.primaryTextColor
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
                color: navItem.selected ? "#FFFFFF" : navItem.accentColor
                implicitWidth: Math.max(22, badgeLabel.implicitWidth + 12)
                implicitHeight: 22

                Label {
                    id: badgeLabel

                    anchors.centerIn: parent
                    text: navItem.badgeText
                    color: navItem.selected ? "#2D79B8" : "#15191D"
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
            color: navItem.primaryTextColor
            font.pixelSize: 13
            font.bold: true
        }

        background: Rectangle {
            color: navItem.elevatedColor
            radius: 10
            border.color: navItem.borderColor
            border.width: 1
        }
    }
}
