import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LumaCore

Item {
    id: page

    property var settingsController
    property bool animationsEnabled: true

    readonly property var themeOptions: ["Auto", "Light", "Dark"]

    function normalizedTheme(value) {
        if (value === "System") {
            return "Auto"
        }
        return themeOptions.indexOf(value) >= 0 ? value : "Dark"
    }

    implicitHeight: content.implicitHeight

    ColumnLayout {
        id: content

        anchors.fill: parent
        spacing: 12

        SectionCard {
            Layout.fillWidth: true
            title: qsTr("Interface")
            subtitle: qsTr("Visual preferences for the app shell")
            surfaceColor: Theme.surface
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
                        color: Theme.primaryText
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Controls sidebar motion, hover transitions, and color fades.")
                        color: Theme.secondaryText
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

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Theme")
                        color: Theme.primaryText
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Auto follows your system light/dark preference.")
                        color: Theme.secondaryText
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }

                ComboBox {
                    Layout.preferredWidth: 180
                    model: page.themeOptions
                    currentIndex: Math.max(0, page.themeOptions.indexOf(
                        page.normalizedTheme(page.settingsController ? page.settingsController.theme : "Dark")))
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
            surfaceColor: Theme.surface
            animationsEnabled: page.animationsEnabled

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Start minimized")
                    color: Theme.primaryText
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
                        color: Theme.primaryText
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Placeholder setting; profile selection will be connected later.")
                        color: Theme.secondaryText
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
