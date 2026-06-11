import QtQuick
import QtQuick.Controls
import LumaCore

Item {
    id: control

    property var segments: []
    property int currentIndex: 0
    property bool animationsEnabled: true
    property real gap: 4

    signal selected(int index)

    implicitWidth: 240
    implicitHeight: 40

    readonly property real segmentWidth: segments.length > 0 ? (width - gap * 2) / segments.length : 0

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: Theme.inputBg
        border.color: Theme.border
        border.width: 1
    }

    Rectangle {
        id: highlight

        width: control.segmentWidth
        height: parent.height - control.gap * 2
        y: control.gap
        x: control.gap + control.currentIndex * control.segmentWidth
        radius: 9
        color: Theme.accent

        Behavior on x {
            NumberAnimation {
                duration: control.animationsEnabled ? 220 : 0
                easing.type: Easing.OutCubic
            }
        }
    }

    Row {
        anchors.fill: parent
        anchors.margins: control.gap

        Repeater {
            model: control.segments

            delegate: Item {
                id: segment

                required property int index
                required property string modelData

                width: control.segmentWidth
                height: parent.height

                Label {
                    anchors.centerIn: parent
                    text: segment.modelData
                    font.pixelSize: 12
                    font.bold: true
                    color: control.currentIndex === segment.index ? "#FFFFFF" : Theme.secondaryText

                    Behavior on color {
                        ColorAnimation {
                            duration: control.animationsEnabled ? 160 : 0
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: control.selected(segment.index)
                }
            }
        }
    }
}
