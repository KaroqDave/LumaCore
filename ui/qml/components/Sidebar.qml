import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

SectionCard {
    id: sidebar

    property bool collapsed: false
    property var treeModel
    property int selectedDeviceIndex: -1
    property int selectedZoneIndex: -1
    property color elevatedColor: "#252B32"
    property color accentColor: "#42A5F5"
    property bool animationsEnabled: true

    signal toggleRequested()
    signal deviceSelected(int deviceIndex)
    signal zoneSelected(int deviceIndex, int zoneIndex)

    title: collapsed ? "" : qsTr("Device Tree")
    subtitle: collapsed ? "" : qsTr("Pick a device or zone")
    cardPadding: collapsed ? 10 : 14

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 10

        ToolButton {
            id: collapseButton

            Layout.fillWidth: true
            Layout.preferredHeight: 38
            text: sidebar.collapsed ? qsTr(">>") : qsTr("Collapse")
            ToolTip.text: sidebar.collapsed ? qsTr("Expand sidebar") : qsTr("Collapse sidebar")
            ToolTip.visible: hovered
            onClicked: sidebar.toggleRequested()

            background: Rectangle {
                radius: 12
                color: collapseButton.down ? "#2A3540" : (collapseButton.hovered ? "#24313B" : sidebar.elevatedColor)
                border.color: collapseButton.hovered ? sidebar.accentColor : sidebar.borderColor
                border.width: 1
            }

            contentItem: Label {
                text: collapseButton.text
                color: sidebar.primaryTextColor
                font.pixelSize: sidebar.collapsed ? 15 : 12
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        DeviceTreePanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !sidebar.collapsed
            treeModel: sidebar.treeModel
            selectedDeviceIndex: sidebar.selectedDeviceIndex
            selectedZoneIndex: sidebar.selectedZoneIndex
            elevatedColor: sidebar.elevatedColor
            accentColor: sidebar.accentColor
            borderColor: sidebar.borderColor
            primaryTextColor: sidebar.primaryTextColor
            secondaryTextColor: sidebar.secondaryTextColor
            animationsEnabled: sidebar.animationsEnabled
            onDeviceSelected: function(deviceIndex) { sidebar.deviceSelected(deviceIndex) }
            onZoneSelected: function(deviceIndex, zoneIndex) { sidebar.zoneSelected(deviceIndex, zoneIndex) }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: sidebar.collapsed
            spacing: 8

            Repeater {
                model: 3

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 38
                    Layout.preferredHeight: 38
                    radius: 12
                    color: index === 0 ? sidebar.accentColor : sidebar.elevatedColor
                    border.color: sidebar.borderColor

                    Label {
                        anchors.centerIn: parent
                        text: index === 0 ? qsTr("D") : qsTr("Z%1").arg(index)
                        color: sidebar.primaryTextColor
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }
}
