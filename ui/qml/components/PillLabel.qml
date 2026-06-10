import QtQuick
import QtQuick.Controls

Rectangle {
    id: pill

    property string text: ""
    property color fillColor: "#24313B"
    property color borderColor: "#344B5D"
    property color textColor: "#BFE3FF"
    property bool animationsEnabled: true

    radius: 999
    color: fillColor
    border.color: borderColor
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
        color: pill.textColor
        font.pixelSize: 11
        font.bold: true
    }
}
