// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import LumaCore

Slider {
    id: control

    property bool animationsEnabled: true
    readonly property int animationDuration: animationsEnabled ? Theme.animationDuration : 0

    hoverEnabled: true

    background: Item {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.availableWidth
        height: 4
        implicitWidth: 200
        implicitHeight: 4

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: Theme.sunken
            opacity: control.enabled ? 1.0 : 0.55
        }

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: height / 2
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
        width: control.hovered || control.pressed ? 18 : 16
        height: width
        radius: width / 2
        color: Theme.surface
        border.color: control.enabled ? Theme.accent : Theme.border
        border.width: 2
        opacity: control.enabled ? 1.0 : 0.35

        Rectangle {
            anchors.centerIn: parent
            width: 6
            height: 6
            radius: 3
            color: Theme.accent
            opacity: control.enabled ? 0.95 : 0.35
        }

        Behavior on width {
            NumberAnimation {
                duration: control.animationDuration
                easing.type: Easing.OutCubic
            }
        }
    }
}
