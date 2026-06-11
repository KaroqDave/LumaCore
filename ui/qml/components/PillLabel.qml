import QtQuick
import QtQuick.Controls
import LumaCore

Rectangle {
    id: pill

    property string text: ""
    property bool animationsEnabled: true

    radius: 999
    color: Theme.pillFill
    border.color: Theme.pillBorder
    implicitWidth: label.implicitWidth + 18
    implicitHeight: 26

    Behavior on color {
        ColorAnimation {
            duration: pill.animationsEnabled ? 120 : 0
        }
    }

    Label {
        id: label

        anchors.centerIn: parent
        text: pill.text
        color: Theme.pillText
        font.pixelSize: 11
        font.bold: true
    }
}
