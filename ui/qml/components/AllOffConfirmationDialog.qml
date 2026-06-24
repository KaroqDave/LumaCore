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

    footer: RowLayout {
        spacing: 8

        Item {
            Layout.fillWidth: true
        }

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
