pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LumaCore

Item {
    id: controls

    property var appController
    property color selectedColor: Theme.defaultColor
    property bool animationsEnabled: true
    property int effectType: 0
    property real effectSpeed: 1.0
    property int brightness: 100
    property var lastResult: ({})
    property string selectedTargetName: ""
    property var targetOptions: []
    property var editDeviceIds: []
    readonly property bool operationPending: appController
        ? appController.pendingDaemonOperations > 0
        : false
    readonly property bool groupTargetSelected: selectedTargetName.length > 0

    signal chooseColorRequested()

    implicitHeight: content.implicitHeight

    function refreshTargetOptions() {
        const names = appController ? appController.deviceGroupNames : []
        const options = [{ "name": "", "label": qsTr("All devices") }]
        for (let index = 0; index < names.length; ++index) {
            options.push({ "name": names[index], "label": names[index] })
        }
        targetOptions = options
        if (targetIndex() < 0) {
            selectedTargetName = ""
        }
    }

    function targetIndex() {
        for (let index = 0; index < targetOptions.length; ++index) {
            if (targetOptions[index].name === selectedTargetName) {
                return index
            }
        }
        return -1
    }

    function targetLabel() {
        const index = targetIndex()
        return index >= 0 ? targetOptions[index].label : qsTr("All devices")
    }

    function openGroupEditor() {
        groupNameField.text = selectedTargetName
        editDeviceIds = selectedTargetName.length > 0 && appController
            ? appController.deviceGroupDeviceIds(selectedTargetName)
            : []
        groupDialog.open()
    }

    function deviceChecked(deviceId) {
        return editDeviceIds.indexOf(deviceId) >= 0
    }

    function setDeviceChecked(deviceId, checked) {
        const ids = editDeviceIds.slice()
        const index = ids.indexOf(deviceId)
        if (checked && index < 0) {
            ids.push(deviceId)
        } else if (!checked && index >= 0) {
            ids.splice(index, 1)
        }
        editDeviceIds = ids
    }

    function applySelectedEffect() {
        if (!appController) {
            return
        }
        if (groupTargetSelected) {
            appController.applyEffectToDeviceGroup(
                selectedTargetName,
                effectType,
                selectedColor,
                effectSpeed,
                brightness)
            return
        }
        appController.applyEffectGlobally(effectType, selectedColor, effectSpeed, brightness)
    }

    function applySelectedBrightness() {
        if (!appController) {
            return
        }
        if (groupTargetSelected) {
            appController.setDeviceGroupBrightness(selectedTargetName, brightness)
            return
        }
        appController.setGlobalBrightness(brightness)
    }

    function allOffSelectedTarget() {
        if (!appController) {
            return
        }
        if (groupTargetSelected) {
            appController.allOffDeviceGroup(selectedTargetName)
            return
        }
        appController.allOffAllDevices()
    }

    function resultSummary(result) {
        return qsTr("%1 applied, %2 skipped, %3 failed.")
            .arg(result.applied || 0)
            .arg(result.skipped || 0)
            .arg(result.failed || 0)
    }

    function colorHex(value) {
        const red = Math.round(value.r * 255).toString(16).padStart(2, "0")
        const green = Math.round(value.g * 255).toString(16).padStart(2, "0")
        const blue = Math.round(value.b * 255).toString(16).padStart(2, "0")
        return ("#" + red + green + blue).toUpperCase()
    }

    Dialog {
        id: resultDialog

        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: controls.lastResult.operation || qsTr("Global operation")
        standardButtons: Dialog.Ok

        contentItem: ColumnLayout {
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: controls.resultSummary(controls.lastResult)
                color: Theme.primaryText
                font.bold: true
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                visible: controls.lastResult.details
                    && controls.lastResult.details.length > 0
                text: visible ? controls.lastResult.details.join("\n") : ""
                color: Theme.secondaryText
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
    }

    Dialog {
        id: allOffDialog

        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: controls.groupTargetSelected
               ? qsTr("Turn off %1?").arg(controls.selectedTargetName)
               : qsTr("Turn off all devices?")
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAccepted: controls.allOffSelectedTarget()

        contentItem: Label {
            text: controls.groupTargetSelected
                  ? qsTr("This sends All Off to writable devices in the selected group. Existing write-confirmation and dry-run rules still apply.")
                  : qsTr("This sends All Off to every writable device. Existing write-confirmation and dry-run rules still apply.")
            color: Theme.primaryText
            wrapMode: Text.WordWrap
        }
    }

    Dialog {
        id: groupDialog

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(620, parent ? parent.width - 48 : 620)
        modal: true
        title: qsTr("Device Groups")
        standardButtons: Dialog.NoButton

        contentItem: ColumnLayout {
            spacing: 12

            TextField {
                id: groupNameField

                Layout.fillWidth: true
                placeholderText: qsTr("Group name")
                color: Theme.primaryText
                selectedTextColor: Theme.accentText
                selectionColor: Theme.accent
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(groupDeviceList.implicitHeight + 20, 260)
                radius: 10
                color: Theme.inputBg
                border.color: Theme.border

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 10
                    clip: true

                    ColumnLayout {
                        id: groupDeviceList

                        width: parent.width
                        spacing: 4

                        Repeater {
                            model: controls.appController ? controls.appController.deviceGroupDeviceOptions() : []

                            delegate: CheckBox {
                                id: deviceOption

                                required property var modelData

                                Layout.fillWidth: true
                                text: qsTr("%1 - %2 zone(s)").arg(modelData.name).arg(modelData.zoneCount)
                                checked: controls.deviceChecked(modelData.id)
                                onToggled: controls.setDeviceChecked(modelData.id, checked)
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: controls.appController
                                     && controls.appController.deviceGroupDeviceOptions().length === 0
                            text: qsTr("No devices are loaded.")
                            color: Theme.secondaryText
                            font.pixelSize: 12
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                AppButton {
                    variant: "secondary"
                    text: qsTr("Delete")
                    enabled: controls.appController
                             && groupNameField.text.length > 0
                             && controls.appController.deviceGroupNames.indexOf(groupNameField.text) >= 0
                    animationsEnabled: controls.animationsEnabled
                    onClicked: {
                        const deletedName = groupNameField.text
                        if (controls.appController.deleteDeviceGroup(deletedName)) {
                            if (controls.selectedTargetName === deletedName) {
                                controls.selectedTargetName = ""
                            }
                            controls.refreshTargetOptions()
                            groupDialog.close()
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                AppButton {
                    variant: "secondary"
                    text: qsTr("Cancel")
                    animationsEnabled: controls.animationsEnabled
                    onClicked: groupDialog.close()
                }

                AppButton {
                    variant: "primary"
                    text: qsTr("Save")
                    enabled: controls.appController !== null
                             && controls.appController !== undefined
                             && groupNameField.text.trim().length > 0
                             && controls.editDeviceIds.length > 0
                    animationsEnabled: controls.animationsEnabled
                    onClicked: {
                        const savedName = groupNameField.text.trim()
                        if (controls.appController.saveDeviceGroup(savedName, controls.editDeviceIds)) {
                            controls.selectedTargetName = savedName
                            controls.refreshTargetOptions()
                            groupDialog.close()
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        id: content

        anchors.fill: parent
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: qsTr("Global Controls")
                    color: Theme.primaryText
                    font.pixelSize: 15
                    font.bold: true
                }

                Label {
                    text: qsTr("Apply across every compatible writable zone.")
                    color: Theme.secondaryText
                    font.pixelSize: 11
                }
            }

            AppButton {
                variant: "secondary"
                text: qsTr("All Off")
                enabled: !controls.operationPending
                animationsEnabled: controls.animationsEnabled
                onClicked: allOffDialog.open()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ComboBox {
                id: targetSelector

                Layout.fillWidth: true
                model: controls.targetOptions
                textRole: "label"
                currentIndex: Math.max(0, controls.targetIndex())
                onActivated: function(index) {
                    controls.selectedTargetName = controls.targetOptions[index].name
                }
            }

            AppButton {
                variant: "secondary"
                text: qsTr("Groups")
                enabled: controls.appController !== null && controls.appController !== undefined
                animationsEnabled: controls.animationsEnabled
                onClicked: controls.openGroupEditor()
            }
        }

        EffectSelector {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            segments: [qsTr("Static"), qsTr("Rainbow"), qsTr("Breathing"), qsTr("Cycle")]
            currentIndex: controls.effectType
            animationsEnabled: controls.animationsEnabled
            onSelected: function(index) { controls.effectType = index }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            AppButton {
                Layout.preferredWidth: 130
                variant: "secondary"
                text: qsTr("Color")
                enabled: controls.effectType === 0 || controls.effectType === 2
                animationsEnabled: controls.animationsEnabled
                onClicked: controls.chooseColorRequested()
            }

            Label {
                text: controls.effectType === 0 || controls.effectType === 2
                      ? controls.colorHex(controls.selectedColor)
                      : qsTr("Generated color")
                color: Theme.secondaryText
                font.family: "monospace"
                font.pixelSize: 11
            }

            Item { Layout.fillWidth: true }

            Label {
                text: controls.brightness + qsTr("%")
                color: Theme.primaryText
                font.family: "monospace"
                font.pixelSize: 11
            }
        }

        AppSlider {
            Layout.fillWidth: true
            from: 0
            to: 100
            stepSize: 1
            value: controls.brightness
            animationsEnabled: controls.animationsEnabled
            onMoved: controls.brightness = Math.round(value)
        }

        RowLayout {
            visible: controls.effectType !== 0
            Layout.fillWidth: true

            Label {
                text: qsTr("Speed")
                color: Theme.secondaryText
                font.pixelSize: 11
            }

            AppSlider {
                Layout.fillWidth: true
                from: 0.1
                to: 5.0
                stepSize: 0.1
                value: controls.effectSpeed
                animationsEnabled: controls.animationsEnabled
                onMoved: controls.effectSpeed = value
            }

            Label {
                text: controls.effectSpeed.toFixed(1) + qsTr("x")
                color: Theme.primaryText
                font.family: "monospace"
                font.pixelSize: 11
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            AppButton {
                Layout.fillWidth: true
                variant: "primary"
                text: controls.groupTargetSelected ? qsTr("Apply to Group") : qsTr("Apply Globally")
                enabled: !controls.operationPending
                animationsEnabled: controls.animationsEnabled
                onClicked: controls.applySelectedEffect()
            }

            AppButton {
                Layout.fillWidth: true
                variant: "secondary"
                text: qsTr("Brightness Only")
                enabled: !controls.operationPending
                animationsEnabled: controls.animationsEnabled
                onClicked: controls.applySelectedBrightness()
            }
        }
    }

    Connections {
        target: controls.appController

        function onGlobalOperationFinished(result) {
            controls.lastResult = result
            resultDialog.open()
        }

        function onDeviceGroupsChanged() {
            controls.refreshTargetOptions()
        }
    }

    Component.onCompleted: controls.refreshTargetOptions()
}
