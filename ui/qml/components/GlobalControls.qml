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
    readonly property bool operationPending: appController
        ? appController.pendingDaemonOperations > 0
        : false

    signal chooseColorRequested()

    implicitHeight: content.implicitHeight

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
        title: qsTr("Turn off all devices?")
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAccepted: controls.appController.allOffAllDevices()

        contentItem: Label {
            text: qsTr("This sends All Off to every writable device. Existing write-confirmation and dry-run rules still apply.")
            color: Theme.primaryText
            wrapMode: Text.WordWrap
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
                text: qsTr("Apply Globally")
                enabled: !controls.operationPending
                animationsEnabled: controls.animationsEnabled
                onClicked: controls.appController.applyEffectGlobally(
                    controls.effectType,
                    controls.selectedColor,
                    controls.effectSpeed,
                    controls.brightness)
            }

            AppButton {
                Layout.fillWidth: true
                variant: "secondary"
                text: qsTr("Brightness Only")
                enabled: !controls.operationPending
                animationsEnabled: controls.animationsEnabled
                onClicked: controls.appController.setGlobalBrightness(controls.brightness)
            }
        }
    }

    Connections {
        target: controls.appController

        function onGlobalOperationFinished(result) {
            controls.lastResult = result
            resultDialog.open()
        }
    }
}
