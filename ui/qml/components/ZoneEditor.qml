// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LumaCore

Item {
    id: editor

    property var appController
    property int selectedDeviceIndex: -1
    property int selectedZoneIndex: -1
    property bool animationsEnabled: true
    readonly property bool hasSelection: selectedDeviceIndex >= 0 && selectedZoneIndex >= 0
    readonly property bool wideLayout: width >= 620
    readonly property bool daemonOperationPending: appController
        ? appController.pendingDaemonOperations > 0
        : false
    readonly property bool selectedDeviceWritable: appController && selectedDeviceIndex >= 0
        ? appController.deviceWritable(selectedDeviceIndex)
        : false
    readonly property string selectedDevicePermissionReason: appController && selectedDeviceIndex >= 0
        ? appController.devicePermissionReason(selectedDeviceIndex)
        : ""

    implicitHeight: content.implicitHeight

    function refresh() {
        if (!appController || !hasSelection) {
            nameField.text = ""
            ledSpin.value = 1
            return
        }

        nameField.text = appController.zoneName(selectedDeviceIndex, selectedZoneIndex)
        ledSpin.value = Math.max(1, appController.zoneLedCount(selectedDeviceIndex, selectedZoneIndex))
    }

    ColumnLayout {
        id: content

        anchors.fill: parent
        spacing: 10

        Label {
            Layout.fillWidth: true
            visible: !editor.hasSelection
            text: qsTr("Pick a zone in the device tree to edit its metadata.")
            color: Theme.secondaryText
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: editor.hasSelection && !editor.selectedDeviceWritable
            text: editor.selectedDevicePermissionReason.length > 0
                ? editor.selectedDevicePermissionReason
                : qsTr("This zone is read-only in the current backend.")
            color: Theme.warning
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        GridLayout {
            Layout.fillWidth: true
            columns: editor.wideLayout ? 4 : 2
            columnSpacing: 10
            rowSpacing: 6

            Label {
                text: qsTr("Zone name")
                color: Theme.secondaryText
                font.pixelSize: 10
            }

            TextField {
                id: nameField

                Layout.fillWidth: true
                Layout.minimumWidth: 210
                enabled: editor.hasSelection && editor.selectedDeviceWritable && !editor.daemonOperationPending
                selectByMouse: true
                placeholderText: qsTr("Zone name")
            }

            Label {
                text: qsTr("LED count")
                color: Theme.secondaryText
                font.pixelSize: 10
            }

            SpinBox {
                id: ledSpin

                Layout.fillWidth: !editor.wideLayout
                Layout.preferredWidth: 160
                enabled: editor.hasSelection && editor.selectedDeviceWritable && !editor.daemonOperationPending
                from: 1
                to: 512
                editable: true
                value: 1
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            AppButton {
                Layout.fillWidth: true
                variant: "primary"
                enabled: editor.hasSelection && editor.selectedDeviceWritable && !editor.daemonOperationPending
                text: qsTr("Save Zone")
                compact: true
                iconName: "check"
                animationsEnabled: editor.animationsEnabled
                onClicked: {
                    if (editor.appController.updateZone(editor.selectedDeviceIndex, editor.selectedZoneIndex, nameField.text, ledSpin.value)) {
                        editor.refresh()
                    }
                }
            }

            AppButton {
                Layout.fillWidth: true
                variant: "secondary"
                enabled: editor.hasSelection
                text: qsTr("Reload")
                compact: true
                animationsEnabled: editor.animationsEnabled
                onClicked: editor.refresh()
            }
        }
    }

    onSelectedDeviceIndexChanged: Qt.callLater(refresh)
    onSelectedZoneIndexChanged: Qt.callLater(refresh)

    Connections {
        target: editor.appController

        function onZoneDataChanged(deviceIndex, zoneIndex) {
            if (deviceIndex === editor.selectedDeviceIndex && (zoneIndex < 0 || zoneIndex === editor.selectedZoneIndex)) {
                editor.refresh()
            }
        }
    }

    Component.onCompleted: refresh()
}
