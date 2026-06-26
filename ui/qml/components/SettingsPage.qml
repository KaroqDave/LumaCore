// SPDX-License-Identifier: GPL-2.0-or-later

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
    property var scheduledProfileNames: []

    function normalizedTheme(value) {
        if (value === "System") {
            return "Auto"
        }
        return themeOptions.indexOf(value) >= 0 ? value : "Dark"
    }

    function refreshScheduledProfileNames() {
        scheduledProfileNames = appController ? appController.profileNames() : []
    }

    function scheduledProfileIndex() {
        if (!settingsController) {
            return -1
        }
        return scheduledProfileNames.indexOf(settingsController.scheduledProfile)
    }

    function scheduledHour() {
        if (!settingsController || settingsController.scheduledProfileTime.length !== 5) {
            return 18
        }
        return Number(settingsController.scheduledProfileTime.slice(0, 2))
    }

    function scheduledMinute() {
        if (!settingsController || settingsController.scheduledProfileTime.length !== 5) {
            return 0
        }
        return Number(settingsController.scheduledProfileTime.slice(3, 5))
    }

    function setScheduledTime(hour, minute) {
        if (!settingsController) {
            return
        }
        const normalizedHour = Math.max(0, Math.min(23, hour))
        const normalizedMinute = Math.max(0, Math.min(59, minute))
        const hourText = normalizedHour < 10 ? "0" + normalizedHour : "" + normalizedHour
        const minuteText = normalizedMinute < 10 ? "0" + normalizedMinute : "" + normalizedMinute
        settingsController.scheduledProfileTime =
                hourText + ":" + minuteText
    }

    Component.onCompleted: refreshScheduledProfileNames()

    Connections {
        target: page.appController

        function onProfilesChanged() {
            page.refreshScheduledProfileNames()
        }
    }

    implicitHeight: content.implicitHeight

    ColumnLayout {
        id: content

        anchors.fill: parent
        spacing: 9

        SectionCard {
            Layout.fillWidth: true
            title: qsTr("Interface")
            subtitle: qsTr("Visual preferences for the app shell")
            surfaceColor: Theme.surface
            animationsEnabled: page.animationsEnabled
            compact: true

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Close to system tray")
                        color: Theme.primaryText
                        font.pixelSize: 12
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: page.settingsController && page.settingsController.trayAvailable
                              ? qsTr("Keeps LumaCore running when its window is closed. Use the tray menu to reopen or quit.")
                              : qsTr("A system tray is not available in this desktop session.")
                        color: Theme.secondaryText
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }

                AppSwitch {
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    enabled: page.settingsController && page.settingsController.trayAvailable
                    checked: page.settingsController ? page.settingsController.closeToTray : false
                    onClicked: {
                        if (page.settingsController) {
                            page.settingsController.closeToTray = checked
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("UI animations")
                        color: Theme.primaryText
                        font.pixelSize: 12
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Controls sidebar motion, hover transitions, and color fades.")
                        color: Theme.secondaryText
                        font.pixelSize: 10
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
                spacing: 10

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
                        text: qsTr("Keeps the window presenting at a steady rate for maximum G-Sync/FreeSync stability. This can increase GPU and power usage; disable it to favor lower power.")
                        color: Theme.secondaryText
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }

                AppSwitch {
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    checked: page.settingsController ? page.settingsController.reduceVrrFlicker : true
                    onClicked: {
                        if (page.settingsController) {
                            page.settingsController.reduceVrrFlicker = checked
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

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
            compact: true

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
            compact: true

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Start minimized")
                    color: Theme.primaryText
                    font.pixelSize: 12
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
                        font.pixelSize: 12
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: page.settingsController && page.settingsController.activeProfile.length > 0
                              ? qsTr("Applies '%1' after discovery. Normal dry-run and write-confirmation rules still apply.")
                                    .arg(page.settingsController.activeProfile)
                              : qsTr("Choose an active profile on the Profiles page first.")
                        color: Theme.secondaryText
                        font.pixelSize: 10
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

        SectionCard {
            Layout.fillWidth: true
            title: qsTr("Schedule")
            subtitle: qsTr("Apply one profile automatically each day while LumaCore is running")
            surfaceColor: Theme.surface
            animationsEnabled: page.animationsEnabled
            compact: true

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Scheduled profile")
                        color: Theme.primaryText
                        font.pixelSize: 12
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: page.scheduledProfileNames.length > 0
                              ? qsTr("Uses the normal dry-run, compatibility, and hardware-confirmation rules.")
                              : qsTr("Create or import a profile before enabling a daily schedule.")
                        color: Theme.secondaryText
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }

                ComboBox {
                    Layout.preferredWidth: 180
                    enabled: page.scheduledProfileNames.length > 0
                    model: page.scheduledProfileNames
                    currentIndex: page.scheduledProfileIndex()
                    displayText: currentIndex >= 0
                                 ? currentText
                                 : qsTr("Choose profile")
                    onActivated: {
                        if (page.settingsController) {
                            page.settingsController.scheduledProfile = currentText
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Daily time")
                        color: Theme.primaryText
                        font.pixelSize: 12
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: page.settingsController && page.settingsController.scheduledProfile.length > 0
                              ? qsTr("Applies '%1' at %2 if LumaCore is open.")
                                    .arg(page.settingsController.scheduledProfile)
                                    .arg(page.settingsController.scheduledProfileTime)
                              : qsTr("Select a profile to choose when it should run.")
                        color: Theme.secondaryText
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }

                RowLayout {
                    spacing: 6

                    SpinBox {
                        Layout.preferredWidth: 72
                        enabled: page.settingsController
                                 && page.settingsController.scheduledProfile.length > 0
                        from: 0
                        to: 23
                        value: page.scheduledHour()
                        editable: true
                        onValueModified: page.setScheduledTime(value, page.scheduledMinute())
                    }

                    Label {
                        text: ":"
                        color: Theme.primaryText
                        font.pixelSize: 16
                        font.bold: true
                    }

                    SpinBox {
                        Layout.preferredWidth: 72
                        enabled: page.settingsController
                                 && page.settingsController.scheduledProfile.length > 0
                        from: 0
                        to: 59
                        value: page.scheduledMinute()
                        editable: true
                        onValueModified: page.setScheduledTime(page.scheduledHour(), value)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Enable daily schedule")
                        color: Theme.primaryText
                        font.pixelSize: 12
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: page.settingsController && page.settingsController.scheduledProfile.length > 0
                              ? qsTr("Runs once per day while LumaCore is open; missed earlier runs are skipped on startup.")
                              : qsTr("A scheduled profile is required before this can be enabled.")
                        color: Theme.secondaryText
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }

                AppSwitch {
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    enabled: page.settingsController
                             && page.settingsController.scheduledProfile.length > 0
                    checked: page.settingsController ? page.settingsController.scheduledProfileEnabled : false
                    onClicked: {
                        if (page.settingsController) {
                            page.settingsController.scheduledProfileEnabled = checked
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
