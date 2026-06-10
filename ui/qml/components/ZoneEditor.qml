import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: editor

    property var appController
    property int selectedDeviceIndex: -1
    property int selectedZoneIndex: -1
    property color selectedColor: "#4080FF"
    property color elevatedColor: "#252B32"
    property color accentColor: "#42A5F5"
    property color borderColor: "#343C44"
    property color primaryTextColor: "#F2F5F8"
    property color secondaryTextColor: "#AEB8C2"
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

    function refresh() {
        if (!appController || !hasSelection) {
            deviceLabel.text = qsTr("No zone selected")
            nameField.text = ""
            typeLabel.text = qsTr("Select a zone from the device tree.")
            ledSpin.value = 1
            currentColorChip.color = "#00000000"
            currentColorLabel.text = qsTr("--")
            return
        }

        deviceLabel.text = appController.deviceName(selectedDeviceIndex)
        nameField.text = appController.zoneName(selectedDeviceIndex, selectedZoneIndex)
        typeLabel.text = appController.zoneTypeName(selectedDeviceIndex, selectedZoneIndex)
        ledSpin.value = Math.max(1, appController.zoneLedCount(selectedDeviceIndex, selectedZoneIndex))
        currentColorChip.color = appController.zoneColor(selectedDeviceIndex, selectedZoneIndex)
        currentColorLabel.text = appController.zoneColorHex(selectedDeviceIndex, selectedZoneIndex)

        editor.effectType = appController.zoneEffectType(selectedDeviceIndex, selectedZoneIndex)
        editor.effectSpeed = appController.zoneEffectSpeed(selectedDeviceIndex, selectedZoneIndex)
        editor.effectBrightness = appController.zoneEffectBrightness(selectedDeviceIndex, selectedZoneIndex)
    }

    implicitHeight: content.implicitHeight

    ColumnLayout {
        id: content

        anchors.fill: parent
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                id: currentColorChip

                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                radius: 14
                color: "#00000000"
                border.color: "#FFFFFF"
                border.width: editor.hasSelection ? 1 : 0

                Behavior on color {
                    ColorAnimation {
                        duration: editor.animationsEnabled ? 160 : 0
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    id: deviceLabel

                    Layout.fillWidth: true
                    color: editor.primaryTextColor
                    font.pixelSize: 15
                    font.bold: true
                    elide: Text.ElideRight
                }

                Label {
                    id: typeLabel

                    Layout.fillWidth: true
                    color: editor.secondaryTextColor
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }

            Label {
                id: currentColorLabel

                Layout.preferredWidth: 84
                color: editor.primaryTextColor
                font.family: "monospace"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignRight
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 10
            rowSpacing: 8

            Label {
                text: qsTr("Zone name")
                color: editor.secondaryTextColor
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
                color: editor.secondaryTextColor
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

            Button {
                Layout.fillWidth: true
                enabled: editor.hasSelection
                text: qsTr("Save Zone")
                onClicked: {
                    if (editor.appController.updateZone(editor.selectedDeviceIndex, editor.selectedZoneIndex, nameField.text, ledSpin.value)) {
                        editor.refresh()
                    }
                }
            }

            Button {
                Layout.fillWidth: true
                enabled: editor.hasSelection
                text: qsTr("Reload")
                onClicked: editor.refresh()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: editor.borderColor
        }

        Label {
            text: qsTr("Effect")
            color: editor.primaryTextColor
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
            accentColor: editor.accentColor
            borderColor: editor.borderColor
            primaryTextColor: editor.primaryTextColor
            secondaryTextColor: editor.secondaryTextColor
            animationsEnabled: editor.animationsEnabled
            onSelected: function(index) { editor.effectType = index }
        }

        // Live animated preview of the selected effect
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            radius: 14
            color: "#171C21"
            border.color: editor.borderColor
            border.width: 1
            clip: true

            // Rainbow gradient backdrop (only meaningful for Rainbow)
            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: 13
                visible: editor.effectType === 1
                opacity: editor.brightnessFactor
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

            Rectangle {
                id: previewSwatch

                property real hue: 0
                property real breath: 1

                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 12
                width: 40
                height: 40
                radius: 12
                border.color: "#FFFFFF"
                border.width: 1

                color: {
                    if (editor.effectType === 1) {
                        return Qt.hsva(hue, 1.0, editor.brightnessFactor, 1.0)
                    }
                    if (editor.effectType === 2) {
                        var f = breath * editor.brightnessFactor
                        return Qt.rgba(editor.selectedColor.r * f, editor.selectedColor.g * f, editor.selectedColor.b * f, 1.0)
                    }
                    var b = editor.brightnessFactor
                    return Qt.rgba(editor.selectedColor.r * b, editor.selectedColor.g * b, editor.selectedColor.b * b, 1.0)
                }

                NumberAnimation on hue {
                    running: editor.effectType === 1 && editor.animationsEnabled
                    from: 0.0
                    to: 1.0
                    loops: Animation.Infinite
                    duration: Math.max(600, 6000 / editor.effectSpeed)
                }

                SequentialAnimation on breath {
                    running: editor.effectType === 2 && editor.animationsEnabled
                    loops: Animation.Infinite

                    NumberAnimation {
                        from: 0.12
                        to: 1.0
                        duration: Math.max(300, 2000 / editor.effectSpeed)
                        easing.type: Easing.InOutSine
                    }
                    NumberAnimation {
                        from: 1.0
                        to: 0.12
                        duration: Math.max(300, 2000 / editor.effectSpeed)
                        easing.type: Easing.InOutSine
                    }
                }
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: previewSwatch.right
                anchors.leftMargin: 12
                anchors.right: parent.right
                anchors.rightMargin: 12
                text: editor.effectType === 1 ? qsTr("Rainbow wave preview")
                    : editor.effectType === 2 ? qsTr("Breathing preview")
                    : qsTr("Static color preview")
                color: editor.effectType === 1 ? "#FFFFFF" : editor.primaryTextColor
                font.pixelSize: 12
                font.bold: true
                elide: Text.ElideRight
            }
        }

        // Base color picker (hidden for Rainbow, which ignores a base color)
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            visible: editor.usesBaseColor

            Rectangle {
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
                radius: 12
                color: editor.selectedColor
                border.color: "#FFFFFF"
                border.width: 1

                Behavior on color {
                    ColorAnimation {
                        duration: editor.animationsEnabled ? 160 : 0
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Base color")
                color: editor.primaryTextColor
                font.pixelSize: 13
                font.bold: true
            }

            Button {
                text: qsTr("Choose")
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
                    color: editor.secondaryTextColor
                    font.pixelSize: 11
                }

                Label {
                    text: editor.effectSpeed.toFixed(1) + qsTr("x")
                    color: editor.primaryTextColor
                    font.family: "monospace"
                    font.pixelSize: 11
                }
            }

            Slider {
                Layout.fillWidth: true
                from: 0.1
                to: 5.0
                stepSize: 0.1
                value: editor.effectSpeed
                onMoved: editor.effectSpeed = value
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
                    color: editor.secondaryTextColor
                    font.pixelSize: 11
                }

                Label {
                    text: editor.effectBrightness + qsTr("%")
                    color: editor.primaryTextColor
                    font.family: "monospace"
                    font.pixelSize: 11
                }
            }

            Slider {
                Layout.fillWidth: true
                from: 0
                to: 100
                stepSize: 1
                value: editor.effectBrightness
                onMoved: editor.effectBrightness = Math.round(value)
            }
        }

        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            enabled: editor.hasSelection
            text: qsTr("Apply Effect to Zone")
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
