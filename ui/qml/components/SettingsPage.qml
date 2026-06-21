import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LumaCore

Item {
    id: page

    property var settingsController
    property var appController
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

                AppSwitch {
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    checked: page.settingsController ? page.settingsController.animationsEnabled : true
                    onClicked: {
                        if (page.settingsController) {
                            page.settingsController.animationsEnabled = checked
                        }
                    }
                }
            }

            RowLayout {
                visible: Qt.platform.os === "windows"
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Reduce VRR flicker")
                        color: Theme.primaryText
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Keeps the window drawing at a steady rate so G-Sync/FreeSync displays don't flicker. Enable only if you notice VRR flicker, as this increases GPU and power usage.")
                        color: Theme.secondaryText
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }

                AppSwitch {
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    checked: page.settingsController ? page.settingsController.reduceVrrFlicker : false
                    onClicked: {
                        if (page.settingsController) {
                            page.settingsController.reduceVrrFlicker = checked
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
            title: qsTr("Safety")
            subtitle: qsTr("Preview write intent without applying changes")
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
                        text: qsTr("Dry-run mode")
                        color: Theme.primaryText
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Logs color and effect changes without updating zones. Useful before real hardware support.")
                        color: Theme.secondaryText
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }

                AppSwitch {
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    checked: page.settingsController ? page.settingsController.dryRunEnabled : false
                    onClicked: {
                        if (page.settingsController) {
                            page.settingsController.dryRunEnabled = checked
                        }
                        if (page.appController) {
                            page.appController.dryRunEnabled = checked
                        }
                    }
                }
            }
        }

        SectionCard {
            Layout.fillWidth: true
            title: qsTr("Startup")
            subtitle: qsTr("Choose how LumaCore behaves when it starts")
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

                AppSwitch {
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
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
                        text: page.settingsController && page.settingsController.activeProfile.length > 0
                              ? qsTr("Applies '%1' after discovery. Normal dry-run and write-confirmation rules still apply.")
                                    .arg(page.settingsController.activeProfile)
                              : qsTr("Choose an active profile on the Profiles page first.")
                        color: Theme.secondaryText
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }

                AppSwitch {
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    enabled: page.settingsController
                             && page.settingsController.activeProfile.length > 0
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
