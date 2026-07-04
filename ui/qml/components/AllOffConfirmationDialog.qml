// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LumaCore

Dialog {
    id: dialog

    property bool selectedZoneMode: false
    property bool groupTargetSelected: false
    property string selectedTargetName: ""
    property bool animationsEnabled: true

    signal confirmed()

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    title: selectedZoneMode
           ? qsTr("Turn off this device?")
           : groupTargetSelected
           ? qsTr("Turn off %1?").arg(selectedTargetName)
           : qsTr("Turn off all devices?")
    standardButtons: Dialog.NoButton
    width: Math.min(540, parent ? parent.width - 48 : 540)
    padding: 18

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusLarge
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
            text: dialog.selectedZoneMode
                  ? qsTr("This sends All Off to the selected device. Existing write-confirmation and dry-run rules still apply.")
                  : dialog.groupTargetSelected
                  ? qsTr("This sends All Off to writable devices in the selected group. Existing write-confirmation and dry-run rules still apply.")
                  : qsTr("This sends All Off to every writable device. Existing write-confirmation and dry-run rules still apply.")
            color: Theme.primaryText
            wrapMode: Text.WordWrap
        }
    }

    footer: Item {
        implicitWidth: footerRow.implicitWidth
        implicitHeight: footerRow.implicitHeight + 12

        RowLayout {
            id: footerRow

            anchors.right: parent.right
            anchors.rightMargin: 18
            spacing: 8

            AppButton {
                variant: "secondary"
                text: qsTr("Cancel")
                compact: true
                animationsEnabled: dialog.animationsEnabled
                onClicked: dialog.close()
            }

            AppButton {
                variant: "primary"
                text: qsTr("All Off")
                compact: true
                animationsEnabled: dialog.animationsEnabled
                onClicked: {
                    dialog.close()
                    dialog.confirmed()
                }
            }
        }
    }
}
