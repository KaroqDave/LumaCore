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

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

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
                text: qsTr("Selected color")
                color: editor.primaryTextColor
                font.pixelSize: 13
                font.bold: true
            }

            Button {
                text: qsTr("Choose")
                onClicked: editor.chooseColorRequested()
            }
        }

        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            enabled: editor.hasSelection
            text: qsTr("Apply Color to Zone")
            onClicked: editor.appController.applyStaticColor(editor.selectedDeviceIndex, editor.selectedZoneIndex, editor.selectedColor)
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
