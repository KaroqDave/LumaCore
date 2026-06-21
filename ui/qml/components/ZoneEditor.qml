pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import LumaCore

Item {
    id: editor

    property var appController
    property int selectedDeviceIndex: -1
    property int selectedZoneIndex: -1
    property color selectedColor: Theme.defaultColor
    property bool animationsEnabled: true
    readonly property bool hasSelection: selectedDeviceIndex >= 0 && selectedZoneIndex >= 0

    // Effect state (0 = Static, 1 = Rainbow, 2 = Breathing, 3 = Color Cycle)
    property int effectType: 0
    property real effectSpeed: 1.0
    property int effectBrightness: 100
    property int confirmationRevision: 0
    property int deviceDiagnosticsRevision: 0
    property bool writeConfirmationOverlayDismissed: false
    readonly property real brightnessFactor: effectBrightness / 100.0
    readonly property bool daemonOperationPending: appController
        ? appController.pendingDaemonOperations > 0
        : false
    readonly property bool usesBaseColor: effectType !== 1 && effectType !== 3
    readonly property bool usesSpeed: effectType !== 0
    readonly property bool selectedDeviceWritable: appController && selectedDeviceIndex >= 0
        ? appController.deviceWritable(selectedDeviceIndex)
        : false
    readonly property string selectedDevicePermissionReason: deviceDiagnosticsRevision >= 0 && appController && selectedDeviceIndex >= 0
        ? appController.devicePermissionReason(selectedDeviceIndex)
        : ""
    readonly property string selectedDeviceHardwareStatus: deviceDiagnosticsRevision >= 0 && appController && selectedDeviceIndex >= 0
        ? appController.deviceLastHardwareWriteStatus(selectedDeviceIndex)
        : ""
    readonly property bool selectedDeviceRequiresConfirmation: confirmationRevision >= 0 && appController && selectedDeviceIndex >= 0
        ? appController.deviceRequiresConfirmation(selectedDeviceIndex)
        : false
    readonly property bool selectedDeviceWriteConfirmed: confirmationRevision >= 0 && appController && selectedDeviceIndex >= 0
        ? appController.deviceWriteConfirmed(selectedDeviceIndex)
        : false
    readonly property var disabledEffectSegments: appController && hasSelection
        ? [
            !appController.deviceSupportsEffect(selectedDeviceIndex, 0),
            !appController.deviceSupportsEffect(selectedDeviceIndex, 1),
            !appController.deviceSupportsEffect(selectedDeviceIndex, 2),
            !appController.deviceSupportsEffect(selectedDeviceIndex, 3)
        ]
        : [true, true, true, true]
    readonly property bool selectedEffectSupported: appController && hasSelection
        ? appController.deviceSupportsEffect(selectedDeviceIndex, effectType)
        : false
    readonly property bool selectedEffectSpeedSupported: appController && hasSelection
        ? appController.deviceSupportsEffectSpeed(selectedDeviceIndex, effectType)
        : false
    readonly property bool selectedEffectBrightnessSupported: appController && hasSelection
        ? appController.deviceSupportsEffectBrightness(selectedDeviceIndex, effectType)
        : false
    readonly property bool writeConfirmationNeeded: appController
        && hasSelection
        && selectedDeviceWritable
        && selectedDeviceRequiresConfirmation
        && !selectedDeviceWriteConfirmed
        && !appController.dryRunEnabled
    readonly property bool writeConfirmationOverlayVisible: writeConfirmationNeeded && !writeConfirmationOverlayDismissed

    signal chooseColorRequested()

    readonly property int rainbowPeriodMs: Math.max(600, 6000 / editor.effectSpeed)
    readonly property int breathingHalfPeriodMs: Math.max(150, 2000 / editor.effectSpeed)

    function restartPreviewAnimations() {
        if (!editor.animationsEnabled) {
            return
        }
        if (editor.effectType === 1 && rainbowClip.visible) {
            rainbowScrollAnim.stop()
            rainbowScrollAnim.start()
        } else if (editor.effectType === 2) {
            breathingAnim.stop()
            breathingAnim.start()
        } else if (editor.effectType === 3) {
            colorCycleAnim.stop()
            colorCycleAnim.start()
        }
    }

    function contrastOn(c) {
        const lum = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b
        return lum > 0.6 ? "#10151A" : "#FFFFFF"
    }

    function hexOf(c) {
        const r = Math.round(c.r * 255).toString(16).padStart(2, "0")
        const g = Math.round(c.g * 255).toString(16).padStart(2, "0")
        const b = Math.round(c.b * 255).toString(16).padStart(2, "0")
        return ("#" + r + g + b).toUpperCase()
    }

    function firstSupportedEffectType() {
        if (!appController || !hasSelection) {
            return 0
        }
        for (let index = 0; index < 4; ++index) {
            if (appController.deviceSupportsEffect(selectedDeviceIndex, index)) {
                return index
            }
        }
        return 0
    }

    function refresh() {
        if (!appController || !hasSelection) {
            nameField.text = ""
            ledSpin.value = 1
            return
        }

        nameField.text = appController.zoneName(selectedDeviceIndex, selectedZoneIndex)
        ledSpin.value = Math.max(1, appController.zoneLedCount(selectedDeviceIndex, selectedZoneIndex))

        editor.effectType = appController.zoneEffectType(selectedDeviceIndex, selectedZoneIndex)
        if (!appController.deviceSupportsEffect(selectedDeviceIndex, editor.effectType)) {
            editor.effectType = firstSupportedEffectType()
        }
        editor.effectSpeed = appController.zoneEffectSpeed(selectedDeviceIndex, selectedZoneIndex)
        editor.effectBrightness = appController.zoneEffectBrightness(selectedDeviceIndex, selectedZoneIndex)
    }

    function applyEffectNow() {
        if (!editor.appController) {
            return false
        }
        return editor.appController.applyEffect(
            editor.selectedDeviceIndex,
            editor.selectedZoneIndex,
            editor.effectType,
            editor.selectedColor,
            editor.effectSpeed,
            editor.effectBrightness)
    }

    function applyEffectSafely() {
        if (!editor.appController || !editor.hasSelection || !editor.selectedEffectSupported) {
            return
        }
        if (!editor.appController.dryRunEnabled && editor.selectedDeviceRequiresConfirmation && !editor.selectedDeviceWriteConfirmed) {
            editor.writeConfirmationOverlayDismissed = false
            return
        }
        applyEffectNow()
    }

    function confirmHardwareWrites() {
        if (!editor.appController) {
            return
        }
        editor.appController.confirmDeviceWrites(editor.selectedDeviceIndex)
    }

    implicitHeight: content.implicitHeight

    ColumnLayout {
        id: content

        anchors.fill: parent
        spacing: 12

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 10
            rowSpacing: 8

            Label {
                text: qsTr("Zone name")
                color: Theme.secondaryText
                font.pixelSize: 11
            }

            TextField {
                id: nameField

                Layout.fillWidth: true
                enabled: editor.hasSelection && editor.selectedDeviceWritable && !editor.daemonOperationPending
                selectByMouse: true
                placeholderText: qsTr("Zone name")
            }

            Label {
                text: qsTr("LED count")
                color: Theme.secondaryText
                font.pixelSize: 11
            }

            SpinBox {
                id: ledSpin

                Layout.fillWidth: true
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
                animationsEnabled: editor.animationsEnabled
                onClicked: editor.refresh()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.divider
        }

        Item {
            id: effectsPane

            Layout.fillWidth: true
            implicitHeight: effectsContent.implicitHeight

            ColumnLayout {
                id: effectsContent

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                enabled: !editor.writeConfirmationOverlayVisible
                opacity: editor.writeConfirmationOverlayVisible ? 0.35 : 1.0
                spacing: 12

                Behavior on opacity {
                    NumberAnimation {
                        duration: editor.animationsEnabled ? 120 : 0
                        easing.type: Easing.OutCubic
                    }
                }

                Label {
                    text: qsTr("Effect")
                    color: Theme.primaryText
                    font.pixelSize: 13
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    visible: editor.hasSelection && !editor.selectedDeviceWritable
                    text: editor.selectedDevicePermissionReason.length > 0
                        ? editor.selectedDevicePermissionReason
                        : qsTr("Read-only discovery device. Effects and zone writes are disabled until a write-capable backend is added.")
                    color: Theme.warning
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    visible: editor.hasSelection && editor.selectedDeviceHardwareStatus.length > 0
                    text: editor.selectedDeviceHardwareStatus
                    color: Theme.secondaryText
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    visible: editor.hasSelection && editor.selectedDeviceWriteConfirmed && !editor.appController.dryRunEnabled
                    text: qsTr("Hardware writes are confirmed for this daemon session. Use All Off if the controller does not respond as expected.")
                    color: Theme.success
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                EffectSelector {
                    id: effectSelector

                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    enabled: editor.hasSelection && editor.selectedDeviceWritable
                    segments: [qsTr("Static"), qsTr("Rainbow"), qsTr("Breathing"), qsTr("Color Cycle")]
                    disabledSegments: editor.disabledEffectSegments
                    currentIndex: editor.effectType
                    animationsEnabled: editor.animationsEnabled
                    onSelected: function(index) {
                        if (editor.appController.deviceSupportsEffect(editor.selectedDeviceIndex, index)) {
                            editor.effectType = index
                        }
                    }
                }

                // Full-width live preview; the whole bar is the color and is clickable
                // to open the picker (for effects that use a base color).
                Rectangle {
                    id: previewBar

                    Layout.fillWidth: true
                    Layout.preferredHeight: 72
                    radius: 14
                    border.color: Theme.border
                    border.width: 1
                    clip: true

                    property real breath: 1
                    property real cycleHue: 0
                    readonly property color staticColor: Qt.rgba(
                        editor.selectedColor.r * editor.brightnessFactor,
                        editor.selectedColor.g * editor.brightnessFactor,
                        editor.selectedColor.b * editor.brightnessFactor, 1.0)
                    readonly property color breathingColor: Qt.rgba(
                        editor.selectedColor.r * breath * editor.brightnessFactor,
                        editor.selectedColor.g * breath * editor.brightnessFactor,
                        editor.selectedColor.b * breath * editor.brightnessFactor, 1.0)

                    color: editor.effectType === 2 ? breathingColor
                         : editor.effectType === 3 ? Qt.hsva(cycleHue, 1, editor.brightnessFactor, 1)
                         : editor.effectType === 0 ? staticColor
                         : Theme.inputBg

                    // Animated rainbow backdrop (only meaningful for Rainbow). Two
                    // seamless gradient tiles scroll horizontally to create the wave.
                    Item {
                        id: rainbowClip

                        anchors.fill: parent
                        anchors.margins: 1
                        visible: editor.effectType === 1
                        opacity: editor.brightnessFactor
                        clip: true

                        Row {
                            id: rainbowRow

                            height: parent.height
                            x: 0

                            Repeater {
                                model: 2

                                Rectangle {
                                    width: Math.max(1, previewBar.width)
                                    height: rainbowRow.height

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
                                id: rainbowScrollAnim

                                target: rainbowRow
                                property: "x"
                                running: editor.effectType === 1 && editor.animationsEnabled && rainbowClip.visible
                                from: 0
                                to: -Math.max(1, previewBar.width)
                                loops: Animation.Infinite
                                duration: editor.rainbowPeriodMs
                            }
                        }
                    }

                    SequentialAnimation {
                        id: breathingAnim

                        running: editor.effectType === 2 && editor.animationsEnabled
                        loops: Animation.Infinite

                        NumberAnimation {
                            target: previewBar
                            property: "breath"
                            from: 0.12
                            to: 1.0
                            duration: editor.breathingHalfPeriodMs
                            easing.type: Easing.InOutSine
                        }
                        NumberAnimation {
                            target: previewBar
                            property: "breath"
                            from: 1.0
                            to: 0.12
                            duration: editor.breathingHalfPeriodMs
                            easing.type: Easing.InOutSine
                        }
                    }

                    NumberAnimation {
                        id: colorCycleAnim

                        target: previewBar
                        property: "cycleHue"
                        running: editor.effectType === 3 && editor.animationsEnabled
                        loops: Animation.Infinite
                        from: 0
                        to: 1
                        duration: editor.rainbowPeriodMs
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 10

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: editor.effectType === 1 ? qsTr("Rainbow wave")
                                    : editor.effectType === 2 ? qsTr("Breathing")
                                    : editor.effectType === 3 ? qsTr("Color cycle")
                                    : qsTr("Static color")
                                color: editor.effectType === 1 || editor.effectType === 3 ? "#FFFFFF" : editor.contrastOn(previewBar.staticColor)
                                font.pixelSize: 13
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: editor.usesBaseColor
                                text: editor.hasSelection ? qsTr("Tap to change color") : ""
                                color: editor.contrastOn(previewBar.staticColor)
                                opacity: 0.8
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                        }

                        Label {
                            visible: editor.usesBaseColor
                            text: editor.hexOf(editor.selectedColor)
                            color: editor.contrastOn(previewBar.staticColor)
                            font.family: "monospace"
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: editor.hasSelection && editor.selectedDeviceWritable && editor.selectedEffectSupported && editor.usesBaseColor
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: editor.chooseColorRequested()
                    }
                }

                // Speed slider (animated effects only)
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    visible: editor.usesSpeed

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Speed")
                            color: editor.selectedEffectSpeedSupported ? Theme.secondaryText : Theme.secondaryText
                            opacity: editor.selectedEffectSpeedSupported ? 1.0 : 0.55
                            font.pixelSize: 11
                        }

                        Label {
                            text: editor.selectedEffectSpeedSupported ? editor.effectSpeed.toFixed(1) + qsTr("x") : qsTr("Fixed")
                            color: Theme.primaryText
                            opacity: editor.selectedEffectSpeedSupported ? 1.0 : 0.55
                            font.family: "monospace"
                            font.pixelSize: 11
                        }
                    }

                    AppSlider {
                        Layout.fillWidth: true
                        enabled: editor.selectedDeviceWritable && editor.selectedEffectSpeedSupported
                        from: 0.1
                        to: 5.0
                        stepSize: 0.1
                        value: editor.effectSpeed
                        animationsEnabled: editor.animationsEnabled
                        onValueChanged: {
                            editor.effectSpeed = value
                            editor.restartPreviewAnimations()
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: editor.selectedDeviceWritable && !editor.selectedEffectSpeedSupported
                        text: qsTr("This controller exposes the selected effect as a native hardware mode with fixed speed.")
                        color: Theme.secondaryText
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }

                // Brightness slider (all effects)
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Brightness")
                            color: Theme.secondaryText
                            opacity: editor.selectedEffectBrightnessSupported ? 1.0 : 0.55
                            font.pixelSize: 11
                        }

                        Label {
                            text: editor.selectedEffectBrightnessSupported ? editor.effectBrightness + qsTr("%") : qsTr("Default")
                            color: Theme.primaryText
                            opacity: editor.selectedEffectBrightnessSupported ? 1.0 : 0.55
                            font.family: "monospace"
                            font.pixelSize: 11
                        }
                    }

                    AppSlider {
                        Layout.fillWidth: true
                        enabled: editor.selectedDeviceWritable && editor.selectedEffectBrightnessSupported
                        from: 0
                        to: 100
                        stepSize: 1
                        value: editor.effectBrightness
                        animationsEnabled: editor.animationsEnabled
                        onValueChanged: editor.effectBrightness = Math.round(value)
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: editor.selectedDeviceWritable && !editor.selectedEffectBrightnessSupported
                        text: qsTr("Brightness is not exposed for this native hardware mode on the selected controller.")
                        color: Theme.secondaryText
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }

                AppButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    variant: "primary"
                    enabled: editor.hasSelection
                        && editor.selectedDeviceWritable
                        && editor.selectedEffectSupported
                        && !editor.daemonOperationPending
                    text: qsTr("Apply Effect to Zone")
                    animationsEnabled: editor.animationsEnabled
                    onClicked: editor.applyEffectSafely()
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    visible: editor.hasSelection && editor.selectedDeviceWritable

                    AppButton {
                        Layout.fillWidth: true
                        variant: "secondary"
                        enabled: editor.selectedDeviceWriteConfirmed
                            && !editor.appController.dryRunEnabled
                            && !editor.daemonOperationPending
                        text: qsTr("All Off")
                        animationsEnabled: editor.animationsEnabled
                        onClicked: editor.appController.allOffDevice(editor.selectedDeviceIndex)
                    }

                    AppButton {
                        Layout.fillWidth: true
                        variant: "secondary"
                        enabled: editor.selectedDeviceWriteConfirmed && !editor.daemonOperationPending
                        text: qsTr("Revoke Confirmation")
                        animationsEnabled: editor.animationsEnabled
                        onClicked: editor.appController.revokeDeviceWrites(editor.selectedDeviceIndex)
                    }
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }

    onEffectTypeChanged: Qt.callLater(restartPreviewAnimations)

    onSelectedDeviceIndexChanged: {
        editor.writeConfirmationOverlayDismissed = false
        Qt.callLater(refresh)
    }
    onSelectedZoneIndexChanged: {
        editor.writeConfirmationOverlayDismissed = false
        Qt.callLater(refresh)
    }

    Connections {
        target: editor.appController

        function onZoneDataChanged(deviceIndex, zoneIndex) {
            if (deviceIndex === editor.selectedDeviceIndex && (zoneIndex < 0 || zoneIndex === editor.selectedZoneIndex)) {
                editor.deviceDiagnosticsRevision += 1
                editor.refresh()
            }
        }

        function onWriteConfirmationChanged(deviceIndex) {
            if (deviceIndex === editor.selectedDeviceIndex) {
                editor.confirmationRevision += 1
                editor.deviceDiagnosticsRevision += 1
                editor.writeConfirmationOverlayDismissed = false
            }
        }
    }

    ShaderEffectSource {
        id: writeConfirmationSource

        x: effectsPane.x
        y: effectsPane.y
        width: effectsPane.width
        height: effectsPane.height
        sourceItem: effectsContent
        live: editor.writeConfirmationOverlayVisible
        recursive: true
        hideSource: editor.writeConfirmationOverlayVisible
        visible: false
    }

    MultiEffect {
        x: effectsPane.x
        y: effectsPane.y
        width: effectsPane.width
        height: effectsPane.height
        z: 19
        visible: editor.writeConfirmationOverlayVisible
        source: writeConfirmationSource
        blurEnabled: true
        blur: 0.9
        blurMax: 24
        saturation: 0.55
    }

    Item {
        id: writeConfirmationOverlay

        x: effectsPane.x
        y: effectsPane.y
        width: effectsPane.width
        height: effectsPane.height
        visible: editor.writeConfirmationOverlayVisible
        z: 20
        opacity: visible ? 1 : 0

        Behavior on opacity {
            NumberAnimation {
                duration: editor.animationsEnabled ? 140 : 0
                easing.type: Easing.OutCubic
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, Theme.dark ? 0.42 : 0.25)
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.max(0, Math.min(parent.width - 32, 520))
            implicitHeight: confirmationContent.implicitHeight + 36
            radius: 20
            color: Theme.surface
            border.color: Theme.selectionBorder
            border.width: 1

            ColumnLayout {
                id: confirmationContent

                anchors.fill: parent
                anchors.margins: 18
                spacing: 12

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Confirm ASUS Aura hardware writes")
                    color: Theme.primaryText
                    font.pixelSize: 17
                    font.bold: true
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("LumaCore is about to enable real ASUS Aura hardware writes for this device for the current daemon session.\n\nAccepting only unlocks the effect controls. No color or effect will be applied until you click Apply Effect to Zone.\n\nStart with low brightness or use All Off immediately if the hardware behaves unexpectedly.")
                    color: Theme.primaryText
                    opacity: 0.92
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    spacing: 10

                    AppButton {
                        Layout.fillWidth: true
                        variant: "secondary"
                        text: qsTr("Decline")
                        animationsEnabled: editor.animationsEnabled
                        enabled: !editor.daemonOperationPending
                        onClicked: editor.writeConfirmationOverlayDismissed = true
                    }

                    AppButton {
                        Layout.fillWidth: true
                        variant: "primary"
                        text: qsTr("Accept")
                        animationsEnabled: editor.animationsEnabled
                        enabled: !editor.daemonOperationPending
                        onClicked: editor.confirmHardwareWrites()
                    }
                }
            }
        }
    }

    Component.onCompleted: refresh()
}
