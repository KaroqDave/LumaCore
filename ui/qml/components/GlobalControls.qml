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
    property int selectedDeviceIndex: -1
    property int selectedZoneIndex: -1
    property string selectedDeviceId: ""
    property string selectedZoneName: ""
    property int effectType: 0
    property real effectSpeed: 1.0
    property int brightness: 100
    property var lastResult: ({})
    property string selectedTargetName: ""
    property var targetOptions: []
    property var editDeviceIds: []
    property int supportRevision: 0
    property bool zoneEffectsEnabled: false
    property int confirmationRevision: 0
    readonly property bool operationPending: appController
        ? appController.pendingDaemonOperations > 0
        : false
    // Confirmation is granted per device; the affordance is offered in zone mode
    // where a single device is selected (matches the legacy ZoneEditor flow).
    readonly property bool selectedDeviceRequiresConfirmation: confirmationRevision >= 0
            && appController && selectedZoneMode && selectedDeviceIndex >= 0
        ? appController.deviceRequiresConfirmation(selectedDeviceIndex)
        : false
    readonly property bool selectedDeviceWriteConfirmed: confirmationRevision >= 0
            && appController && selectedZoneMode && selectedDeviceIndex >= 0
        ? appController.deviceWriteConfirmed(selectedDeviceIndex)
        : false
    readonly property bool zoneSelected: selectedDeviceIndex >= 0 && selectedZoneIndex >= 0
    readonly property bool selectedZoneMode: zoneSelected && zoneEffectsEnabled
    readonly property bool groupTargetSelected: selectedTargetName.length > 0
    readonly property bool selectedEffectSupported: targetSupportsEffect(effectType)
    readonly property bool selectedEffectSpeedSupported: targetSupportsEffectSpeed(effectType)
    readonly property bool selectedEffectBrightnessSupported: targetSupportsEffectBrightness(effectType)
    readonly property bool colorEditable: selectedEffectSupported && (effectType === 0 || effectType === 2)
    readonly property bool usesBaseColor: effectType !== 1 && effectType !== 3
    readonly property bool usesSpeed: effectType !== 0
    readonly property real brightnessFactor: brightness / 100.0
    readonly property int rainbowPeriodMs: Math.max(600, 6000 / effectSpeed)
    readonly property int breathingHalfPeriodMs: Math.max(150, 2000 / effectSpeed)
    readonly property var disabledEffectSegments: [
        !targetSupportsEffect(0),
        !targetSupportsEffect(1),
        !targetSupportsEffect(2),
        !targetSupportsEffect(3)
    ]

    signal chooseColorRequested()
    signal selectedColorSyncRequested(color colorValue)

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

    function targetSupportsEffect(index) {
        if (supportRevision < 0) {
            return false
        }
        if (!appController) {
            return false
        }
        return selectedZoneMode
            ? appController.zoneSupportsEffect(selectedDeviceIndex, selectedZoneIndex, index)
            : appController.globalTargetSupportsEffect(selectedTargetName, index)
    }

    function targetSupportsEffectSpeed(index) {
        if (supportRevision < 0) {
            return false
        }
        if (!appController) {
            return false
        }
        return selectedZoneMode
            ? appController.zoneSupportsEffectSpeed(selectedDeviceIndex, selectedZoneIndex, index)
            : appController.globalTargetSupportsEffectSpeed(selectedTargetName, index)
    }

    function targetSupportsEffectBrightness(index) {
        if (supportRevision < 0) {
            return false
        }
        if (!appController) {
            return false
        }
        return selectedZoneMode
            ? appController.zoneSupportsEffectBrightness(selectedDeviceIndex, selectedZoneIndex, index)
            : appController.globalTargetSupportsEffectBrightness(selectedTargetName, index)
    }

    function refreshSupport() {
        ++supportRevision
        if (selectedEffectSupported) {
            return
        }
        for (let index = 0; index < 4; ++index) {
            if (targetSupportsEffect(index)) {
                effectType = index
                return
            }
        }
    }

    function loadSelectedZoneEffect() {
        if (!appController || !zoneSelected) {
            return
        }
        effectType = appController.zoneEffectType(selectedDeviceIndex, selectedZoneIndex)
        effectSpeed = appController.zoneEffectSpeed(selectedDeviceIndex, selectedZoneIndex)
        brightness = appController.zoneEffectBrightness(selectedDeviceIndex, selectedZoneIndex)
        selectedColorSyncRequested(appController.zoneEffectColor(selectedDeviceIndex, selectedZoneIndex))
        refreshSupport()
    }

    function loadZoneEffectsToggle() {
        zoneEffectsEnabled = appController && zoneSelected
            ? appController.zoneEffectsPanelEnabled(selectedDeviceIndex, selectedZoneIndex)
            : false
        zoneEffectsSwitch.checked = zoneEffectsEnabled
        if (selectedZoneMode) {
            loadSelectedZoneEffect()
        } else {
            refreshSupport()
        }
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
        if (selectedZoneMode) {
            appController.applyEffect(
                selectedDeviceIndex,
                selectedZoneIndex,
                effectType,
                selectedColor,
                effectSpeed,
                brightness)
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
        if (selectedZoneMode) {
            appController.setZoneBrightness(selectedDeviceIndex, selectedZoneIndex, brightness)
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
        if (selectedZoneMode) {
            // Use the dedicated device all-off command rather than writing a
            // Static(black) effect: All Off must work even for zones that do
            // not support the Static effect type.
            appController.allOffDevice(selectedDeviceIndex)
            return
        }
        if (groupTargetSelected) {
            appController.allOffDeviceGroup(selectedTargetName)
            return
        }
        appController.allOffAllDevices()
    }

    function confirmHardwareWrites() {
        if (!appController || selectedDeviceIndex < 0) {
            return
        }
        if (appController.confirmDeviceWrites(selectedDeviceIndex)) {
            confirmationRevision += 1
        }
    }

    function revokeHardwareWrites() {
        if (!appController || selectedDeviceIndex < 0) {
            return
        }
        if (appController.revokeDeviceWrites(selectedDeviceIndex)) {
            confirmationRevision += 1
        }
    }

    function resultSummary(result) {
        return qsTr("%1 applied, %2 skipped, %3 failed.")
            .arg(result.applied || 0)
            .arg(result.skipped || 0)
            .arg(result.failed || 0)
    }

    function contrastOn(c) {
        const lum = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b
        return lum > 0.6 ? "#10151A" : "#FFFFFF"
    }

    function restartPreviewAnimations() {
        if (!controls.animationsEnabled) {
            return
        }
        if (controls.effectType === 1 && globalRainbowClip.visible) {
            globalRainbowScrollAnim.stop()
            globalRainbowScrollAnim.start()
        } else if (controls.effectType === 2) {
            globalBreathingAnim.stop()
            globalBreathingAnim.start()
        } else if (controls.effectType === 3) {
            globalColorCycleAnim.stop()
            globalColorCycleAnim.start()
        }
    }

    OperationResultDialog {
        id: resultDialog

        operationResult: controls.lastResult
        summary: controls.resultSummary(controls.lastResult)
    }

    AllOffConfirmationDialog {
        id: allOffDialog

        selectedZoneMode: controls.selectedZoneMode
        groupTargetSelected: controls.groupTargetSelected
        selectedTargetName: controls.selectedTargetName
        animationsEnabled: controls.animationsEnabled
        onConfirmed: controls.allOffSelectedTarget()
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
                    compact: true
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
                    compact: true
                    animationsEnabled: controls.animationsEnabled
                    onClicked: groupDialog.close()
                }

                AppButton {
                    variant: "primary"
                    text: qsTr("Save")
                    compact: true
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
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Label {
                    text: qsTr("Global Controls")
                    color: Theme.primaryText
                    font.pixelSize: 14
                    font.bold: true
                }

                Label {
                    text: controls.selectedZoneMode
                          ? qsTr("Apply effects to the selected zone.")
                          : qsTr("Apply across every compatible writable zone.")
                    color: Theme.secondaryText
                    font.pixelSize: 10
                }
            }

            Rectangle {
                visible: controls.zoneSelected
                Layout.preferredWidth: 172
                Layout.preferredHeight: 36
                radius: 8
                color: controls.selectedZoneMode ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16) : Theme.inputBg
                border.color: controls.selectedZoneMode ? Theme.selectionBorder : Theme.border
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 8
                    spacing: 8

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Effects")
                            color: controls.selectedZoneMode ? Theme.accent : Theme.primaryText
                            font.pixelSize: 10
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: controls.selectedZoneName.length > 0 ? controls.selectedZoneName : qsTr("Selected zone")
                            color: Theme.secondaryText
                            font.pixelSize: 9
                            elide: Text.ElideRight
                        }
                    }

                    AppSwitch {
                        id: zoneEffectsSwitch

                        checked: controls.zoneEffectsEnabled
                        animationsEnabled: controls.animationsEnabled
                        onClicked: {
                            controls.zoneEffectsEnabled = checked
                            if (controls.appController) {
                                controls.appController.setZoneEffectsPanelEnabled(
                                    controls.selectedDeviceIndex,
                                    controls.selectedZoneIndex,
                                    checked)
                            }
                            if (checked) {
                                controls.loadSelectedZoneEffect()
                            } else {
                                controls.refreshSupport()
                            }
                        }
                    }
                }
            }

            ComboBox {
                id: targetSelector

                visible: !controls.selectedZoneMode
                Layout.preferredWidth: 190
                model: controls.targetOptions
                textRole: "label"
                currentIndex: Math.max(0, controls.targetIndex())
                onActivated: function(index) {
                    controls.selectedTargetName = controls.targetOptions[index].name
                    controls.refreshSupport()
                }
            }

            AppButton {
                variant: "secondary"
                text: qsTr("All Off")
                compact: true
                enabled: !controls.operationPending
                animationsEnabled: controls.animationsEnabled
                onClicked: allOffDialog.open()
            }

            AppButton {
                visible: !controls.selectedZoneMode
                variant: "secondary"
                text: qsTr("Groups")
                compact: true
                enabled: controls.appController !== null && controls.appController !== undefined
                animationsEnabled: controls.animationsEnabled
                onClicked: controls.openGroupEditor()
            }
        }

        EffectSelector {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            segments: [qsTr("Static"), qsTr("Rainbow"), qsTr("Breathing"), qsTr("Cycle")]
            disabledSegments: controls.disabledEffectSegments
            currentIndex: controls.effectType
            animationsEnabled: controls.animationsEnabled
            compact: true
            onSelected: function(index) {
                if (controls.targetSupportsEffect(index)) {
                    controls.effectType = index
                }
            }
        }

        Rectangle {
            id: globalPreviewBar

            Layout.fillWidth: true
            Layout.preferredHeight: 58
            radius: 8
            border.color: previewMouse.containsMouse && controls.colorEditable ? Theme.accent : Theme.border
            border.width: 1
            clip: true
            opacity: controls.selectedEffectSupported ? 1.0 : 0.5

            property real breath: 1
            property real cycleHue: 0
            readonly property color staticColor: Qt.rgba(
                controls.selectedColor.r * controls.brightnessFactor,
                controls.selectedColor.g * controls.brightnessFactor,
                controls.selectedColor.b * controls.brightnessFactor, 1.0)
            readonly property color breathingColor: Qt.rgba(
                controls.selectedColor.r * breath * controls.brightnessFactor,
                controls.selectedColor.g * breath * controls.brightnessFactor,
                controls.selectedColor.b * breath * controls.brightnessFactor, 1.0)

            color: controls.effectType === 2 ? breathingColor
                 : controls.effectType === 3 ? Qt.hsva(cycleHue, 1, controls.brightnessFactor, 1)
                 : controls.effectType === 0 ? staticColor
                 : Theme.inputBg

            Item {
                id: globalRainbowClip

                anchors.fill: parent
                anchors.margins: 1
                visible: controls.effectType === 1
                opacity: controls.brightnessFactor
                clip: true

                Row {
                    id: globalRainbowRow

                    height: parent.height
                    x: 0

                    Repeater {
                        model: 2

                        Rectangle {
                            width: Math.max(1, globalPreviewBar.width)
                            height: globalRainbowRow.height

                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: "#FF0000" }
                                GradientStop { position: 0.17; color: "#FFFF00" }
                                GradientStop { position: 0.34; color: "#00FF00" }
                                GradientStop { position: 0.5; color: "#00FFFF" }
                                GradientStop { position: 0.67; color: "#0000FF" }
                                GradientStop { position: 0.84; color: "#FF00FF" }
                                GradientStop { position: 1.0; color: "#FF0000" }
                            }
                        }
                    }

                    NumberAnimation {
                        id: globalRainbowScrollAnim

                        target: globalRainbowRow
                        property: "x"
                        running: controls.effectType === 1 && controls.animationsEnabled && globalRainbowClip.visible
                        from: 0
                        to: -Math.max(1, globalPreviewBar.width)
                        loops: Animation.Infinite
                        duration: controls.rainbowPeriodMs
                    }
                }
            }

            SequentialAnimation {
                id: globalBreathingAnim

                running: controls.effectType === 2 && controls.animationsEnabled
                loops: Animation.Infinite

                NumberAnimation {
                    target: globalPreviewBar
                    property: "breath"
                    from: 0.12
                    to: 1.0
                    duration: controls.breathingHalfPeriodMs
                    easing.type: Easing.InOutSine
                }
                NumberAnimation {
                    target: globalPreviewBar
                    property: "breath"
                    from: 1.0
                    to: 0.12
                    duration: controls.breathingHalfPeriodMs
                    easing.type: Easing.InOutSine
                }
            }

            NumberAnimation {
                id: globalColorCycleAnim

                target: globalPreviewBar
                property: "cycleHue"
                running: controls.effectType === 3 && controls.animationsEnabled
                loops: Animation.Infinite
                from: 0
                to: 1
                duration: controls.rainbowPeriodMs
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: controls.effectType === 1 ? qsTr("Rainbow wave")
                            : controls.effectType === 2 ? qsTr("Breathing")
                            : controls.effectType === 3 ? qsTr("Color cycle")
                            : qsTr("Static color")
                        color: controls.effectType === 1 || controls.effectType === 3
                               ? "#FFFFFF"
                               : controls.contrastOn(globalPreviewBar.staticColor)
                        font.pixelSize: 12
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: controls.usesBaseColor && controls.colorEditable
                        text: qsTr("Tap to change color")
                        color: controls.contrastOn(globalPreviewBar.staticColor)
                        opacity: 0.8
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }

                Label {
                    visible: controls.usesBaseColor
                    text: Theme.colorToHex(controls.selectedColor)
                    color: controls.contrastOn(globalPreviewBar.staticColor)
                    font.family: "monospace"
                    font.pixelSize: 11
                    font.bold: true
                }
            }

            MouseArea {
                id: previewMouse

                anchors.fill: parent
                hoverEnabled: true
                enabled: controls.colorEditable
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: controls.chooseColorRequested()
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 1
            columnSpacing: 10
            rowSpacing: 4

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: qsTr("Brightness")
                    color: Theme.secondaryText
                    font.pixelSize: 10
                }

                AppSlider {
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    stepSize: 1
                    value: controls.brightness
                    enabled: controls.selectedEffectBrightnessSupported
                    animationsEnabled: controls.animationsEnabled
                    onMoved: controls.brightness = Math.round(value)
                }

                Label {
                    text: controls.brightness + qsTr("%")
                    color: Theme.primaryText
                    opacity: controls.selectedEffectBrightnessSupported ? 1.0 : 0.45
                    font.family: "monospace"
                    font.pixelSize: 10
                }
            }

            RowLayout {
                visible: controls.effectType !== 0
                Layout.fillWidth: true
                Layout.columnSpan: 1
                spacing: 8

                Label {
                    text: qsTr("Speed")
                    color: Theme.secondaryText
                    font.pixelSize: 10
                }

                AppSlider {
                    Layout.fillWidth: true
                    from: 0.1
                    to: 5.0
                    stepSize: 0.1
                    value: controls.effectSpeed
                    enabled: controls.selectedEffectSpeedSupported
                    animationsEnabled: controls.animationsEnabled
                    onMoved: controls.effectSpeed = value
                }

                Label {
                    text: controls.selectedEffectSpeedSupported
                          ? controls.effectSpeed.toFixed(1) + qsTr("x")
                          : qsTr("Fixed")
                    color: Theme.primaryText
                    opacity: controls.selectedEffectSpeedSupported ? 1.0 : 0.55
                    font.family: "monospace"
                    font.pixelSize: 10
                }
            }
        }

        Rectangle {
            id: confirmationBanner

            Layout.fillWidth: true
            visible: controls.selectedZoneMode
                     && controls.selectedDeviceRequiresConfirmation
                     && controls.appController
                     && !controls.appController.dryRunEnabled
            Layout.preferredHeight: visible ? confirmationColumn.implicitHeight + 20 : 0
            radius: 8
            color: controls.selectedDeviceWriteConfirmed ? Theme.inputBg : Theme.warningBg
            border.color: controls.selectedDeviceWriteConfirmed ? Theme.accent : Theme.warning
            border.width: 1

            ColumnLayout {
                id: confirmationColumn

                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: controls.selectedDeviceWriteConfirmed
                          ? qsTr("Hardware writes are confirmed for this device.")
                          : qsTr("This device requires confirmation before real hardware writes are sent.")
                    color: Theme.primaryText
                    wrapMode: Text.WordWrap
                    font.pixelSize: 11
                }

                AppButton {
                    Layout.fillWidth: true
                    variant: controls.selectedDeviceWriteConfirmed ? "secondary" : "primary"
                    text: controls.selectedDeviceWriteConfirmed
                          ? qsTr("Revoke Confirmation")
                          : qsTr("Confirm Hardware Writes")
                    compact: true
                    enabled: !controls.operationPending
                    animationsEnabled: controls.animationsEnabled
                    onClicked: {
                        if (controls.selectedDeviceWriteConfirmed) {
                            controls.revokeHardwareWrites()
                        } else {
                            controls.confirmHardwareWrites()
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            AppButton {
                Layout.fillWidth: true
                variant: "primary"
                text: controls.selectedZoneMode
                      ? qsTr("Apply to Zone")
                      : controls.groupTargetSelected ? qsTr("Apply to Group") : qsTr("Apply Globally")
                compact: true
                iconName: "check"
                enabled: !controls.operationPending && controls.selectedEffectSupported
                animationsEnabled: controls.animationsEnabled
                onClicked: controls.applySelectedEffect()
            }

            AppButton {
                Layout.fillWidth: true
                variant: "secondary"
                text: qsTr("Brightness Only")
                compact: true
                enabled: !controls.operationPending && controls.selectedEffectBrightnessSupported
                animationsEnabled: controls.animationsEnabled
                onClicked: controls.applySelectedBrightness()
            }
        }
    }

    onEffectTypeChanged: Qt.callLater(restartPreviewAnimations)
    onEffectSpeedChanged: Qt.callLater(restartPreviewAnimations)
    onSelectedDeviceIndexChanged: Qt.callLater(loadZoneEffectsToggle)
    onSelectedZoneIndexChanged: Qt.callLater(loadZoneEffectsToggle)
    onAppControllerChanged: Qt.callLater(loadZoneEffectsToggle)

    Connections {
        target: controls.appController

        function onGlobalOperationFinished(result) {
            controls.lastResult = result
            resultDialog.open()
        }

        function onDeviceGroupsChanged() {
            controls.refreshTargetOptions()
            controls.refreshSupport()
        }

        function onBackendInfoChanged() {
            controls.refreshSupport()
        }

        function onDaemonDevicesRefreshed() {
            controls.refreshSupport()
            controls.loadZoneEffectsToggle()
            controls.confirmationRevision += 1
        }

        function onWriteConfirmationChanged(deviceIndex) {
            if (deviceIndex === controls.selectedDeviceIndex) {
                controls.confirmationRevision += 1
            }
        }

        function onZoneDataChanged(deviceIndex, zoneIndex) {
            if (controls.selectedZoneMode
                    && deviceIndex === controls.selectedDeviceIndex
                    && (zoneIndex < 0 || zoneIndex === controls.selectedZoneIndex)) {
                controls.loadSelectedZoneEffect()
            } else {
                controls.refreshSupport()
            }
        }
    }

    Component.onCompleted: {
        controls.refreshTargetOptions()
        controls.loadZoneEffectsToggle()
    }
}
