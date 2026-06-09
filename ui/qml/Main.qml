import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root

    width: 960
    height: 640
    visible: true
    title: qsTr("LumaCore")

    property int redValue: Math.round(redSlider.value)
    property int greenValue: Math.round(greenSlider.value)
    property int blueValue: Math.round(blueSlider.value)
    readonly property color previewColor: Qt.rgba(redValue / 255, greenValue / 255, blueValue / 255, 1)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 18

        Label {
            text: qsTr("LumaCore")
            font.pixelSize: 30
            font.bold: true
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Mock-only RGB control. No real hardware access is used in this build.")
            color: palette.mid
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 24

            GroupBox {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: qsTr("Devices and Zones")

                ListView {
                    id: zoneList

                    anchors.fill: parent
                    clip: true
                    model: deviceModel
                    currentIndex: count > 0 ? 0 : -1

                    delegate: ItemDelegate {
                        required property int index
                        required property string deviceName
                        required property string zoneName
                        required property int ledCount
                        required property string zoneColorHex

                        width: ListView.view.width
                        highlighted: ListView.isCurrentItem
                        onClicked: zoneList.currentIndex = index

                        contentItem: Column {
                            spacing: 4

                            Label {
                                text: zoneName
                                font.bold: true
                            }

                            Label {
                                text: qsTr("%1 · %2 LEDs · %3").arg(deviceName).arg(ledCount).arg(zoneColorHex)
                                color: palette.mid
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            GroupBox {
                Layout.preferredWidth: 360
                Layout.fillHeight: true
                title: qsTr("Static Color")

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 18

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 120
                        radius: 10
                        color: root.previewColor
                        border.color: palette.mid
                    }

                    Label {
                        text: qsTr("#%1%2%3")
                            .arg(root.redValue.toString(16).padStart(2, "0"))
                            .arg(root.greenValue.toString(16).padStart(2, "0"))
                            .arg(root.blueValue.toString(16).padStart(2, "0"))
                            .toUpperCase()
                        font.family: "monospace"
                        font.pixelSize: 20
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 3
                        rowSpacing: 12
                        columnSpacing: 12

                        Label { text: qsTr("Red") }
                        Slider {
                            id: redSlider
                            Layout.fillWidth: true
                            from: 0
                            to: 255
                            stepSize: 1
                            value: 64
                        }
                        Label { text: root.redValue }

                        Label { text: qsTr("Green") }
                        Slider {
                            id: greenSlider
                            Layout.fillWidth: true
                            from: 0
                            to: 255
                            stepSize: 1
                            value: 128
                        }
                        Label { text: root.greenValue }

                        Label { text: qsTr("Blue") }
                        Slider {
                            id: blueSlider
                            Layout.fillWidth: true
                            from: 0
                            to: 255
                            stepSize: 1
                            value: 255
                        }
                        Label { text: root.blueValue }
                    }

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("Apply Static Color")
                        enabled: zoneList.currentIndex >= 0
                        onClicked: deviceModel.setZoneColor(zoneList.currentIndex, root.redValue, root.greenValue, root.blueValue)
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }
        }
    }
}
