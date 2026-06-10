import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    property var settingsController
    property color elevatedColor: "#252B32"
    property color borderColor: "#343C44"
    property color primaryTextColor: "#F2F5F8"
    property color secondaryTextColor: "#AEB8C2"
    property bool animationsEnabled: true

    implicitHeight: content.implicitHeight

    ColumnLayout {
        id: content

        anchors.fill: parent
        spacing: 12

        SectionCard {
            Layout.fillWidth: true
            title: qsTr("Interface")
            subtitle: qsTr("Visual preferences for the app shell")
            surfaceColor: page.elevatedColor
            borderColor: page.borderColor
            primaryTextColor: page.primaryTextColor
            secondaryTextColor: page.secondaryTextColor
            animationsEnabled: page.animationsEnabled

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("UI animations")
                        color: page.primaryTextColor
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Controls sidebar motion, hover transitions, and color fades.")
                        color: page.secondaryTextColor
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }

                Switch {
                    checked: page.settingsController ? page.settingsController.animationsEnabled : true
                    onClicked: {
                        if (page.settingsController) {
                            page.settingsController.animationsEnabled = checked
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Theme")
                    color: page.primaryTextColor
                    font.pixelSize: 13
                    font.bold: true
                }

                ComboBox {
                    Layout.preferredWidth: 180
                    model: ["Dark", "System", "Light"]
                    currentIndex: Math.max(0, find(page.settingsController ? page.settingsController.theme : "Dark"))
                    onActivated: {
                        if (page.settingsController) {
                            page.settingsController.theme = currentText
                        }
                    }
                }
            }
        }

        SectionCard {
            Layout.fillWidth: true
            title: qsTr("Startup")
            subtitle: qsTr("Stored now for later launch behavior")
            surfaceColor: page.elevatedColor
            borderColor: page.borderColor
            primaryTextColor: page.primaryTextColor
            secondaryTextColor: page.secondaryTextColor
            animationsEnabled: page.animationsEnabled

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Start minimized")
                    color: page.primaryTextColor
                    font.pixelSize: 13
                    font.bold: true
                }

                Switch {
                    checked: page.settingsController ? page.settingsController.startMinimized : false
                    onClicked: {
                        if (page.settingsController) {
                            page.settingsController.startMinimized = checked
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Apply profile on launch")
                        color: page.primaryTextColor
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Placeholder setting; profile selection will be connected later.")
                        color: page.secondaryTextColor
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }

                Switch {
                    checked: page.settingsController ? page.settingsController.applyOnLaunch : false
                    onClicked: {
                        if (page.settingsController) {
                            page.settingsController.applyOnLaunch = checked
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
