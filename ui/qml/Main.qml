import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: root

    width: 1120
    height: 760
    visible: true
    title: qsTr("LumaCore")

    property color selectedColor: "#4080FF"

    function colorToHex(value) {
        const red = Math.round(value.r * 255).toString(16).padStart(2, "0")
        const green = Math.round(value.g * 255).toString(16).padStart(2, "0")
        const blue = Math.round(value.b * 255).toString(16).padStart(2, "0")
        return ("#" + red + green + blue).toUpperCase()
    }

    ColorDialog {
        id: colorDialog
        title: qsTr("Choose Static Color")
        selectedColor: root.selectedColor
        onAccepted: root.selectedColor = selectedColor
    }

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
                Layout.preferredWidth: 320
                Layout.fillHeight: true
                title: qsTr("Devices")

                ListView {
                    id: deviceList

                    anchors.fill: parent
                    clip: true
                    model: deviceModel
                    currentIndex: count > 0 ? 0 : -1
                    onCurrentIndexChanged: {
                        zoneModel.deviceIndex = currentIndex
                        zoneList.currentIndex = zoneList.count > 0 ? 0 : -1
                    }

                    delegate: ItemDelegate {
                        required property int index
                        required property string deviceName
                        required property string vendor
                        required property string deviceType
                        required property int zoneCount

                        width: ListView.view.width
                        highlighted: ListView.isCurrentItem
                        onClicked: deviceList.currentIndex = index

                        contentItem: Column {
                            spacing: 4

                            Label {
                                text: deviceName
                                font.bold: true
                            }

                            Label {
                                text: qsTr("%1 · %2 · %3 zone(s)").arg(vendor).arg(deviceType).arg(zoneCount)
                                color: palette.mid
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            GroupBox {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: qsTr("Zones")

                ListView {
                    id: zoneList

                    anchors.fill: parent
                    clip: true
                    model: zoneModel
                    currentIndex: count > 0 ? 0 : -1

                    delegate: ItemDelegate {
                        required property int index
                        required property string zoneName
                        required property string zoneType
                        required property int ledCount
                        required property string zoneColorHex

                        width: ListView.view.width
                        highlighted: ListView.isCurrentItem
                        onClicked: zoneList.currentIndex = index

                        contentItem: RowLayout {
                            spacing: 12

                            Rectangle {
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                radius: 6
                                color: zoneColorHex
                                border.color: palette.mid
                            }

                            Column {
                                Layout.fillWidth: true
                                spacing: 4

                                Label {
                                    text: zoneName
                                    font.bold: true
                                }

                                Label {
                                    text: qsTr("%1 · %2 LEDs · %3").arg(zoneType).arg(ledCount).arg(zoneColorHex)
                                    color: palette.mid
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.preferredWidth: 360
                Layout.fillHeight: true
                spacing: 18

                GroupBox {
                    Layout.fillWidth: true
                    title: qsTr("Static Color")

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 14

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 96
                            radius: 10
                            color: root.selectedColor
                            border.color: palette.mid
                        }

                        Label {
                            text: root.colorToHex(root.selectedColor)
                            font.family: "monospace"
                            font.pixelSize: 20
                        }

                        Button {
                            Layout.fillWidth: true
                            text: qsTr("Choose Color")
                            onClicked: colorDialog.open()
                        }

                        Button {
                            Layout.fillWidth: true
                            text: qsTr("Apply to Selected Zone")
                            enabled: deviceList.currentIndex >= 0 && zoneList.currentIndex >= 0
                            onClicked: appController.applyStaticColor(deviceList.currentIndex, zoneList.currentIndex, root.selectedColor)
                        }
                    }
                }

                GroupBox {
                    Layout.fillWidth: true
                    title: qsTr("Profiles")

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        TextField {
                            id: profileNameField
                            Layout.fillWidth: true
                            text: "default"
                            placeholderText: qsTr("Profile name")
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Button {
                                Layout.fillWidth: true
                                text: qsTr("Save")
                                onClicked: {
                                    if (appController.saveProfile(profileNameField.text)) {
                                        profileBox.model = appController.profileNames()
                                    }
                                }
                            }

                            Button {
                                Layout.fillWidth: true
                                text: qsTr("Refresh")
                                onClicked: profileBox.model = appController.profileNames()
                            }
                        }

                        ComboBox {
                            id: profileBox
                            Layout.fillWidth: true
                            model: appController.profileNames()
                        }

                        Button {
                            Layout.fillWidth: true
                            text: qsTr("Load Selected Profile")
                            enabled: profileBox.currentText.length > 0
                            onClicked: appController.loadProfile(profileBox.currentText)
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Stored in: %1").arg(appController.profilesDirectory)
                            color: palette.mid
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                GroupBox {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Status / Log")

                    ColumnLayout {
                        anchors.fill: parent

                        Label {
                            Layout.fillWidth: true
                            text: appController.statusMessage
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }

                        TextArea {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            text: appController.logText
                            readOnly: true
                            wrapMode: TextArea.Wrap
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        zoneModel.deviceIndex = deviceList.currentIndex
    }
}
