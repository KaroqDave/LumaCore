// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import LumaCore

Button {
    id: control

    // "primary" | "secondary"
    property string variant: "secondary"
    property string iconName: ""
    property bool animationsEnabled: true
    property bool compact: false
    property int controlHeight: compact ? 34 : 40
    readonly property bool primary: variant === "primary"
    readonly property int animationDuration: animationsEnabled ? 140 : 0
    readonly property color foregroundColor: control.primary
        ? Theme.accentText
        : (control.enabled ? Theme.primaryText : Theme.mutedText)

    implicitHeight: controlHeight
    leftPadding: iconName.length > 0 && text.length === 0 ? 10 : (compact ? 12 : 16)
    rightPadding: iconName.length > 0 && text.length === 0 ? 10 : (compact ? 12 : 16)
    font.pixelSize: compact ? 12 : 13
    font.bold: true
    hoverEnabled: true

    contentItem: Item {
        implicitWidth: contentRow.implicitWidth
        implicitHeight: contentRow.implicitHeight

        Row {
            id: contentRow

            anchors.centerIn: parent
            spacing: 8

            NavIcon {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.iconName.length > 0
                width: 18
                height: 18
                name: control.iconName
                color: control.foregroundColor
                strokeWidth: 2
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.text.length > 0
                text: control.text
                font: control.font
                color: control.foregroundColor
            }
        }
    }

    background: Rectangle {
        id: bg

        radius: Theme.radiusSmall
        border.width: control.primary ? 0 : 1
        border.color: control.primary
                      ? "transparent"
                      : (control.hovered ? Theme.accentSoftBorder : Theme.border)
        opacity: control.enabled ? 1.0 : 0.45
        scale: control.down ? 0.985 : 1.0

        color: control.primary
               ? (control.down ? Theme.accentPressed
                                : (control.hovered ? Theme.accentHover : Theme.accent))
               : (control.down ? Theme.hoverStrong
                                : (control.hovered ? Theme.elevated : Theme.subtleSurface))

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

        Behavior on scale {
            NumberAnimation {
                duration: control.animationsEnabled ? 90 : 0
                easing.type: Easing.OutCubic
            }
        }
    }
}
