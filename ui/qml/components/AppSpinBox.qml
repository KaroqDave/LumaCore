// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import LumaCore

SpinBox {
    id: control

    implicitHeight: 32

    contentItem: TextInput {
        z: 2
        text: control.displayText
        font: control.font
        color: control.enabled ? Theme.primaryText : Theme.secondaryText
        selectionColor: Theme.accent
        selectedTextColor: Theme.accentText
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        clip: true
    }

    up.indicator: Rectangle {
        x: control.mirrored ? 0 : control.width - width
        height: control.height
        width: 28
        radius: Theme.radiusSmall
        color: control.up.pressed ? Theme.accentSoft : "transparent"

        Text {
            anchors.centerIn: parent
            text: "+"
            color: control.enabled ? Theme.primaryText : Theme.secondaryText
            font.pixelSize: 14
            font.bold: true
        }
    }

    down.indicator: Rectangle {
        x: control.mirrored ? control.width - width : 0
        height: control.height
        width: 28
        radius: Theme.radiusSmall
        color: control.down.pressed ? Theme.accentSoft : "transparent"

        Text {
            anchors.centerIn: parent
            text: "−"
            color: control.enabled ? Theme.primaryText : Theme.secondaryText
            font.pixelSize: 14
            font.bold: true
        }
    }

    background: Rectangle {
        implicitWidth: 120
        radius: Theme.radiusSmall
        color: Theme.sunken
        border.color: Theme.border
        border.width: 1
    }
}
