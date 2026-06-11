import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LumaCore

Item {
    id: panel

    property var treeModel
    property int selectedDeviceIndex: -1
    property int selectedZoneIndex: -1
    property bool animationsEnabled: true

    signal deviceSelected(int deviceIndex)
    signal zoneSelected(int deviceIndex, int zoneIndex)

    TreeView {
        id: tree

        anchors.fill: parent
        clip: true
        model: panel.treeModel
        boundsBehavior: Flickable.StopAtBounds
        columnWidthProvider: function() { return width }

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        delegate: TreeViewDelegate {
            id: treeDelegate

            readonly property bool deviceNode: model.isDevice === true
            readonly property bool zoneNode: model.isZone === true
            readonly property int sourceDeviceIndex: typeof model.deviceIndex === "number" ? model.deviceIndex : -1
            readonly property int sourceZoneIndex: typeof model.zoneIndex === "number" ? model.zoneIndex : -1
            readonly property bool selectedNode: zoneNode
                                             ? panel.selectedDeviceIndex === sourceDeviceIndex && panel.selectedZoneIndex === sourceZoneIndex
                                             : panel.selectedDeviceIndex === sourceDeviceIndex && panel.selectedZoneIndex < 0

            implicitWidth: tree.width
            implicitHeight: deviceNode ? 48 : 42
            text: model.displayName || ""
            onClicked: {
                if (deviceNode) {
                    tree.toggleExpanded(row)
                    panel.deviceSelected(sourceDeviceIndex)
                } else if (zoneNode) {
                    panel.zoneSelected(sourceDeviceIndex, sourceZoneIndex)
                }
            }

            ToolTip.text: model.description || ""
            ToolTip.visible: hovered && ToolTip.text.length > 0

            background: Rectangle {
                radius: 12
                color: treeDelegate.selectedNode ? Theme.selectionBg : (treeDelegate.hovered ? Theme.hover : "transparent")
                border.color: treeDelegate.selectedNode ? Theme.selectionBorder : "transparent"
                border.width: 1

                Behavior on color {
                    ColorAnimation {
                        duration: panel.animationsEnabled ? 120 : 0
                    }
                }
            }

            contentItem: RowLayout {
                spacing: 10

                Item {
                    Layout.preferredWidth: Math.max(4, treeDelegate.depth * 16)
                    Layout.fillHeight: true
                }

                Rectangle {
                    Layout.preferredWidth: treeDelegate.zoneNode ? 18 : 20
                    Layout.preferredHeight: treeDelegate.zoneNode ? 18 : 20
                    radius: treeDelegate.zoneNode ? 6 : 8
                    color: treeDelegate.zoneNode ? (model.zoneColorHex || Theme.accent) : Theme.elevated
                    border.color: treeDelegate.selectedNode ? "#FFFFFF" : Theme.border
                    border.width: 1
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Label {
                        Layout.fillWidth: true
                        text: model.displayName || ""
                        color: treeDelegate.selectedNode ? Theme.selectionText : Theme.primaryText
                        font.pixelSize: treeDelegate.deviceNode ? 13 : 12
                        font.bold: treeDelegate.deviceNode
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: model.description || ""
                        color: treeDelegate.selectedNode ? Theme.selectionSubText : Theme.secondaryText
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }
            }
        }

        Component.onCompleted: expand(0)
    }
}
