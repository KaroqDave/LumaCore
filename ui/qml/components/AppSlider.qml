import QtQuick
import QtQuick.Controls
import LumaCore

Slider {
    id: control

    property bool animationsEnabled: true
    readonly property int animationDuration: animationsEnabled ? Theme.animationDuration : 0

    background: Item {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.availableWidth
        height: 6
        implicitWidth: 200
        implicitHeight: 6

        Rectangle {
            anchors.fill: parent
            radius: 3
            color: Theme.inputBg
            border.color: Theme.border
            border.width: 1
            opacity: control.enabled ? 1.0 : 0.55
        }

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 3
            color: Theme.accent
            opacity: control.enabled ? 1.0 : 0.25

            Behavior on width {
                NumberAnimation {
                    duration: control.animationDuration
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        width: 18
        height: 18
        radius: 9
        color: Theme.accent
        border.color: Theme.accentBottom
        border.width: 1
        opacity: control.enabled ? 1.0 : 0.35

        Rectangle {
            anchors.centerIn: parent
            width: 6
            height: 6
            radius: 3
            color: "#FFFFFF"
            opacity: 0.85
        }
    }
}
