// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import LumaCore

Control {
    id: control

    signal clicked()

    property bool checked: false
    property bool animationsEnabled: true
    readonly property int animationDuration: animationsEnabled ? Theme.animationDuration : 0

    implicitWidth: 44
    implicitHeight: 24

    padding: 0

    contentItem: Item {
        implicitWidth: control.implicitWidth
        implicitHeight: control.implicitHeight

        Rectangle {
            id: track

            anchors.fill: parent
            radius: height / 2
            color: control.checked ? Theme.accent : Theme.sunken
            border.color: control.checked ? "transparent" : Theme.border
            border.width: 1

            Behavior on color {
                ColorAnimation {
                    duration: control.animationDuration
                }
            }

            Behavior on border.color {
                ColorAnimation {
                    duration: control.animationDuration
                }
            }
        }

        Rectangle {
            id: handle

            x: control.checked ? track.width - width - 3 : 3
            anchors.verticalCenter: parent.verticalCenter
            width: 18
            height: 18
            radius: width / 2
            color: control.checked ? "#FFFFFF" : Theme.elevated
            border.color: control.checked ? "#FFFFFF" : Theme.border
            border.width: 1

            Behavior on x {
                NumberAnimation {
                    duration: control.animationDuration
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            control.checked = !control.checked
            control.clicked()
        }
    }
}
