// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LumaCore

Dialog {
    id: dialog

    property var operationResult: ({})
    property string summary: ""

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    title: operationResult.operation || qsTr("Global operation")
    standardButtons: Dialog.NoButton
    width: Math.min(520, parent ? parent.width - 48 : 520)
    padding: 18

    background: Rectangle {
        color: Theme.surface
        radius: 8
        border.color: Theme.border
        border.width: 1
    }

    header: Label {
        text: dialog.title
        color: Theme.primaryText
        font.pixelSize: 17
        font.bold: true
        padding: 18
        leftPadding: 18
        rightPadding: 18
        topPadding: 18
        bottomPadding: 0
    }

    contentItem: ColumnLayout {
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: dialog.summary
            color: Theme.primaryText
            font.bold: true
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: dialog.operationResult.details
                && dialog.operationResult.details.length > 0
            text: visible ? dialog.operationResult.details.join("\n") : ""
            color: Theme.secondaryText
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
    }

    footer: Item {
        implicitWidth: closeButton.implicitWidth
        implicitHeight: closeButton.implicitHeight + 12

        AppButton {
            id: closeButton

            anchors.horizontalCenter: parent.horizontalCenter
            width: 140
            variant: "primary"
            text: qsTr("OK")
            compact: true
            onClicked: dialog.close()
        }
    }

    // The custom footer replaces the standard DialogButtonBox, which would otherwise make OK
    // the default button. Restore Enter/Return as a dismiss affordance while the dialog is open.
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: dialog.opened
        onActivated: dialog.close()
    }
}
