import QtQuick
import QtQuick.Controls
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

    // Effect state (0 = Static, 1 = Rainbow, 2 = Breathing)
    property int effectType: 0
    property real effectSpeed: 1.0
    property int effectBrightness: 100
    readonly property real brightnessFactor: effectBrightness / 100.0
    readonly property bool usesBaseColor: effectType !== 1
    readonly property bool usesSpeed: effectType !== 0

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

    function refresh() {
        if (!appController || !hasSelection) {
            nameField.text = ""
            ledSpin.value = 1
            return
        }

        nameField.text = appController.zoneName(selectedDeviceIndex, selectedZoneIndex)
        ledSpin.value = Math.max(1, appController.zoneLedCount(selectedDeviceIndex, selectedZoneIndex))

        editor.effectType = appController.zoneEffectType(selectedDeviceIndex, selectedZoneIndex)
        editor.effectSpeed = appController.zoneEffectSpeed(selectedDeviceIndex, selectedZoneIndex)
        editor.effectBrightness = appController.zoneEffectBrightness(selectedDeviceIndex, selectedZoneIndex)
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
                enabled: editor.hasSelection
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
                enabled: editor.hasSelection
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
                enabled: editor.hasSelection
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

        Label {
            text: qsTr("Effect")
            color: Theme.primaryText
            font.pixelSize: 13
            font.bold: true
        }

        EffectSelector {
            id: effectSelector

            Layout.fillWidth: true
            Layout.preferredHeight: 40
            enabled: editor.hasSelection
            segments: [qsTr("Static"), qsTr("Rainbow"), qsTr("Breathing")]
            currentIndex: editor.effectType
            animationsEnabled: editor.animationsEnabled
            onSelected: function(index) { editor.effectType = index }
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
            readonly property color staticColor: Qt.rgba(
                editor.selectedColor.r * editor.brightnessFactor,
                editor.selectedColor.g * editor.brightnessFactor,
                editor.selectedColor.b * editor.brightnessFactor, 1.0)
            readonly property color breathingColor: Qt.rgba(
                editor.selectedColor.r * breath * editor.brightnessFactor,
                editor.selectedColor.g * breath * editor.brightnessFactor,
                editor.selectedColor.b * breath * editor.brightnessFactor, 1.0)

            color: editor.effectType === 2 ? breathingColor
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

                    NumberAnimation on x {
                        id: rainbowScrollAnim

                        running: editor.effectType === 1 && editor.animationsEnabled && rainbowClip.visible
                        from: 0
                        to: -Math.max(1, previewBar.width)
                        loops: Animation.Infinite
                        duration: editor.rainbowPeriodMs
                    }
                }
            }

            SequentialAnimation on breath {
                id: breathingAnim

                running: editor.effectType === 2 && editor.animationsEnabled
                loops: Animation.Infinite

                NumberAnimation {
                    from: 0.12
                    to: 1.0
                    duration: editor.breathingHalfPeriodMs
                    easing.type: Easing.InOutSine
                }
                NumberAnimation {
                    from: 1.0
                    to: 0.12
                    duration: editor.breathingHalfPeriodMs
                    easing.type: Easing.InOutSine
                }
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
                            : qsTr("Static color")
                        color: editor.effectType === 1 ? "#FFFFFF" : editor.contrastOn(previewBar.staticColor)
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
                enabled: editor.hasSelection && editor.usesBaseColor
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
                    color: Theme.secondaryText
                    font.pixelSize: 11
                }

                Label {
                    text: editor.effectSpeed.toFixed(1) + qsTr("x")
                    color: Theme.primaryText
                    font.family: "monospace"
                    font.pixelSize: 11
                }
            }

            AppSlider {
                Layout.fillWidth: true
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
                    font.pixelSize: 11
                }

                Label {
                    text: editor.effectBrightness + qsTr("%")
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
                value: editor.effectBrightness
                animationsEnabled: editor.animationsEnabled
                onValueChanged: editor.effectBrightness = Math.round(value)
            }
        }

        AppButton {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            variant: "primary"
            enabled: editor.hasSelection
            text: qsTr("Apply Effect to Zone")
            animationsEnabled: editor.animationsEnabled
            onClicked: editor.appController.applyEffect(
                editor.selectedDeviceIndex,
                editor.selectedZoneIndex,
                editor.effectType,
                editor.selectedColor,
                editor.effectSpeed,
                editor.effectBrightness)
        }

        Item {
            Layout.fillHeight: true
        }
    }

    onEffectTypeChanged: Qt.callLater(restartPreviewAnimations)

    onSelectedDeviceIndexChanged: Qt.callLater(refresh)
    onSelectedZoneIndexChanged: Qt.callLater(refresh)

    Connections {
        target: editor.appController

        function onZoneDataChanged(deviceIndex, zoneIndex) {
            if (deviceIndex === editor.selectedDeviceIndex && zoneIndex === editor.selectedZoneIndex) {
                editor.refresh()
            }
        }
    }

    Component.onCompleted: refresh()
}
