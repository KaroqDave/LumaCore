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
            readonly property color zoneSwatchColor: model.zoneColorHex || Theme.accent

            implicitWidth: tree.width
            implicitHeight: deviceNode ? 48 : 40
            leftPadding: 10
            rightPadding: 12
            topPadding: 6
            bottomPadding: 6
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

            // Chevron lives in contentItem so it aligns with the icon + text block.
            indicator: Item {
                implicitWidth: 0
                implicitHeight: 0
            }

            background: Rectangle {
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                radius: 10
                color: treeDelegate.selectedNode
                       ? Theme.elevated
                       : (treeDelegate.hovered ? Theme.hover : "transparent")
                border.color: treeDelegate.selectedNode ? Theme.border : "transparent"
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
                    visible: treeDelegate.deviceNode && treeDelegate.hasChildren
                    Layout.preferredWidth: visible ? 16 : 0
                    Layout.preferredHeight: 16
                    Layout.alignment: Qt.AlignVCenter

                    Item {
                        anchors.centerIn: parent
                        width: 12
                        height: 12
                        rotation: treeDelegate.expanded ? 90 : 0

                        Behavior on rotation {
                            NumberAnimation {
                                duration: panel.animationsEnabled ? 160 : 0
                                easing.type: Easing.OutCubic
                            }
                        }

                        NavIcon {
                            anchors.fill: parent
                            name: "chevron"
                            color: treeDelegate.selectedNode ? Theme.primaryText : Theme.secondaryText
                            strokeWidth: 2
                        }
                    }
                }

                Item {
                    visible: treeDelegate.zoneNode
                    Layout.preferredWidth: 28
                    Layout.fillHeight: true
                    Layout.alignment: Qt.AlignVCenter

                    Rectangle {
                        x: 12
                        y: 0
                        width: 1
                        height: parent.height
                        color: Theme.treeLine
                    }

                    Rectangle {
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        width: 12
                        height: 1
                        color: Theme.treeLine
                    }
                }

                Item {
                    visible: treeDelegate.deviceNode
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    Layout.alignment: Qt.AlignVCenter

                    NavIcon {
                        anchors.centerIn: parent
                        width: 20
                        height: 20
                        name: "motherboard"
                        color: treeDelegate.selectedNode ? Theme.primaryText : Theme.secondaryText
                        strokeWidth: 1.8
                    }
                }

                Rectangle {
                    visible: treeDelegate.zoneNode
                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 26
                    Layout.alignment: Qt.AlignVCenter
                    radius: 8
                    color: treeDelegate.zoneSwatchColor
                    border.color: treeDelegate.selectedNode
                                  ? Theme.selectionBorder
                                  : Qt.darker(treeDelegate.zoneSwatchColor, 1.15)
                    border.width: treeDelegate.selectedNode ? 2 : 1
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: model.displayName || ""
                        color: Theme.primaryText
                        font.pixelSize: treeDelegate.deviceNode ? 13 : 12
                        font.bold: treeDelegate.deviceNode || treeDelegate.selectedNode
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: (model.description || "").length > 0
                        text: model.description || ""
                        color: Theme.secondaryText
                        font.pixelSize: 10
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Label {
                    visible: treeDelegate.zoneNode
                    text: model.zoneColorHex || ""
                    color: Theme.secondaryText
                    font.family: "monospace"
                    font.pixelSize: 10
                    font.bold: true
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }

        Component.onCompleted: expand(0)
    }
}
