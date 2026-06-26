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
    standardButtons: Dialog.Ok

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
}
