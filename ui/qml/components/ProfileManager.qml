import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import LumaCore

Item {
    id: manager

    property var appController
    property var settingsController
    property bool animationsEnabled: true
    property var compatibilityReport: ({})
    property var applyResultReport: ({})
    property string pendingProfile: ""
    property string pendingSaveProfile: ""
    property string pendingDeleteProfile: ""

    function refreshProfiles(preferredProfile) {
        if (!appController) {
            profileBox.model = []
            return
        }

        const previous = preferredProfile || profileBox.currentText
        profileBox.model = appController.profileNames()
        const index = profileBox.find(previous)
        if (index >= 0) {
            profileBox.currentIndex = index
        } else if (profileBox.count > 0) {
            profileBox.currentIndex = 0
        }
    }

    function normalizedProfileName(value) {
        const normalized = value.trim().replace(/[^A-Za-z0-9_-]+/g, "_")
        return normalized.length > 0 ? normalized : "default"
    }

    implicitHeight: content.implicitHeight

    FileDialog {
        id: importDialog

        title: qsTr("Import LumaCore Profile")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("LumaCore profiles (*.json)")]
        onAccepted: {
            const importedName = manager.appController.importProfile(selectedFile)
            if (importedName.length > 0) {
                manager.refreshProfiles(importedName)
                profileNameField.text = importedName
            }
        }
    }

    FileDialog {
        id: exportDialog

        title: qsTr("Export LumaCore Profile")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [qsTr("LumaCore profiles (*.json)")]
        onAccepted: manager.appController.exportProfile(profileBox.currentText, selectedFile)
    }

    Dialog {
        id: compatibilityDialog

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(600, parent ? parent.width - 48 : 600)
        modal: true
        title: qsTr("Profile Compatibility")
        standardButtons: Dialog.NoButton

        contentItem: ColumnLayout {
            spacing: 12

            Label {
                Layout.fillWidth: true
                text: manager.pendingProfile.length > 0
                      ? qsTr("Profile: %1").arg(manager.pendingProfile)
                      : ""
                color: Theme.primaryText
                font.pixelSize: 15
                font.bold: true
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: manager.compatibilityReport.summary || qsTr("Compatibility information is unavailable.")
                color: Theme.primaryText
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            Rectangle {
                visible: manager.compatibilityReport.details
                         && manager.compatibilityReport.details.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? Math.min(detailsLabel.implicitHeight + 20, 180) : 0
                radius: 10
                color: Theme.elevated
                border.color: Theme.border

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 10
                    clip: true

                    Label {
                        id: detailsLabel

                        width: parent.width
                        text: manager.compatibilityReport.details
                              ? manager.compatibilityReport.details.join("\n")
                              : ""
                        color: Theme.secondaryText
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Label {
                visible: !manager.compatibilityReport.canApply
                Layout.fillWidth: true
                text: qsTr("This profile has no compatible zones to apply.")
                color: Theme.warning
                font.pixelSize: 12
                font.bold: true
                wrapMode: Text.WordWrap
            }
        }

        footer: RowLayout {
            spacing: 10

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                variant: "secondary"
                text: qsTr("Cancel")
                animationsEnabled: manager.animationsEnabled
                onClicked: compatibilityDialog.close()
            }

            AppButton {
                variant: "primary"
                enabled: manager.compatibilityReport.canApply === true
                text: qsTr("Apply Profile")
                animationsEnabled: manager.animationsEnabled
                onClicked: {
                    compatibilityDialog.close()
                    manager.applyResultReport =
                        manager.appController.applyProfileWithReport(manager.pendingProfile)
                    applyResultDialog.open()
                }
            }
        }
    }

    Dialog {
        id: applyResultDialog

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(600, parent ? parent.width - 48 : 600)
        modal: true
        title: manager.applyResultReport.success
               ? (manager.applyResultReport.partial
                  ? qsTr("Profile Partially Applied")
                  : qsTr("Profile Applied"))
               : qsTr("Profile Could Not Be Applied")
        standardButtons: Dialog.NoButton

        contentItem: ColumnLayout {
            spacing: 12

            Label {
                Layout.fillWidth: true
                text: manager.applyResultReport.summary || manager.applyResultReport.error || ""
                color: Theme.primaryText
                font.pixelSize: 13
                font.bold: true
                wrapMode: Text.WordWrap
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 18
                rowSpacing: 6

                Label {
                    text: qsTr("Applied")
                    color: Theme.secondaryText
                }
                Label {
                    text: manager.applyResultReport.appliedZones || 0
                    color: Theme.success
                    font.bold: true
                }
                Label {
                    text: qsTr("Missing")
                    color: Theme.secondaryText
                }
                Label {
                    text: (manager.applyResultReport.missingDeviceZones || 0)
                          + (manager.applyResultReport.missingZones || 0)
                    color: Theme.warning
                    font.bold: true
                }
                Label {
                    text: qsTr("Invalid or unsupported")
                    color: Theme.secondaryText
                }
                Label {
                    text: (manager.applyResultReport.invalidZones || 0)
                          + (manager.applyResultReport.unsupportedZones || 0)
                    color: Theme.warning
                    font.bold: true
                }
                Label {
                    text: qsTr("Write failures")
                    color: Theme.secondaryText
                }
                Label {
                    text: manager.applyResultReport.failedZones || 0
                    color: manager.applyResultReport.failedZones > 0 ? Theme.warning : Theme.secondaryText
                    font.bold: true
                }
            }

            Rectangle {
                visible: manager.applyResultReport.details
                         && manager.applyResultReport.details.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? Math.min(resultDetailsLabel.implicitHeight + 20, 180) : 0
                radius: 10
                color: Theme.elevated
                border.color: Theme.border

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 10
                    clip: true

                    Label {
                        id: resultDetailsLabel

                        width: parent.width
                        text: manager.applyResultReport.details
                              ? manager.applyResultReport.details.join("\n")
                              : ""
                        color: Theme.secondaryText
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        footer: Item {
            implicitHeight: resultCloseButton.implicitHeight + 8

            AppButton {
                id: resultCloseButton

                anchors.horizontalCenter: parent.horizontalCenter
                variant: "primary"
                text: qsTr("Close")
                animationsEnabled: manager.animationsEnabled
                onClicked: applyResultDialog.close()
            }
        }
    }

    Dialog {
        id: overwriteDialog

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(480, parent ? parent.width - 48 : 480)
        modal: true
        title: qsTr("Overwrite Profile?")
        standardButtons: Dialog.NoButton

        contentItem: Label {
            text: qsTr("A profile named '%1' already exists. Saving will replace its stored device and zone settings.")
                  .arg(manager.pendingSaveProfile)
            color: Theme.primaryText
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        footer: RowLayout {
            spacing: 10

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                variant: "secondary"
                text: qsTr("Cancel")
                animationsEnabled: manager.animationsEnabled
                onClicked: overwriteDialog.close()
            }

            AppButton {
                variant: "primary"
                text: qsTr("Overwrite")
                animationsEnabled: manager.animationsEnabled
                onClicked: {
                    overwriteDialog.close()
                    if (manager.appController.saveProfile(manager.pendingSaveProfile)) {
                        manager.refreshProfiles(manager.pendingSaveProfile)
                        profileNameField.text = manager.pendingSaveProfile
                    }
                }
            }
        }
    }

    Dialog {
        id: deleteDialog

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(480, parent ? parent.width - 48 : 480)
        modal: true
        title: qsTr("Delete Profile?")
        standardButtons: Dialog.NoButton

        contentItem: ColumnLayout {
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: qsTr("Delete profile '%1'? This cannot be undone.")
                      .arg(manager.pendingDeleteProfile)
                color: Theme.primaryText
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            Label {
                visible: manager.settingsController
                         && manager.settingsController.activeProfile === manager.pendingDeleteProfile
                Layout.fillWidth: true
                text: qsTr("This is the active profile. Deleting it will also disable Apply profile on launch.")
                color: Theme.warning
                font.pixelSize: 12
                font.bold: true
                wrapMode: Text.WordWrap
            }
        }

        footer: RowLayout {
            spacing: 10

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                variant: "secondary"
                text: qsTr("Cancel")
                animationsEnabled: manager.animationsEnabled
                onClicked: deleteDialog.close()
            }

            AppButton {
                variant: "primary"
                text: qsTr("Delete")
                animationsEnabled: manager.animationsEnabled
                onClicked: {
                    deleteDialog.close()
                    const deletedName = manager.pendingDeleteProfile
                    if (manager.appController.deleteProfile(deletedName)) {
                        if (manager.settingsController
                                && manager.settingsController.activeProfile === deletedName) {
                            manager.settingsController.activeProfile = ""
                        }
                        profileNameField.text = "default"
                        manager.refreshProfiles()
                    }
                }
            }
        }
    }

    ColumnLayout {
        id: content

        anchors.fill: parent
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: qsTr("Save, restore, and choose the profile LumaCore should treat as active.")
            color: Theme.secondaryText
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            TextField {
                id: profileNameField

                Layout.fillWidth: true
                text: profileBox.currentText.length > 0 ? profileBox.currentText : "default"
                placeholderText: qsTr("Profile name")
                selectByMouse: true
            }

            AppButton {
                variant: "primary"
                text: qsTr("Save")
                animationsEnabled: manager.animationsEnabled
                onClicked: {
                    const normalizedName = manager.normalizedProfileName(profileNameField.text)
                    if (manager.appController.profileExists(normalizedName)) {
                        manager.pendingSaveProfile = normalizedName
                        overwriteDialog.open()
                    } else if (manager.appController.saveProfile(normalizedName)) {
                        manager.refreshProfiles(normalizedName)
                    }
                }
            }
        }

        ComboBox {
            id: profileBox

            Layout.fillWidth: true
            onActivated: profileNameField.text = currentText
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: manager.settingsController && manager.settingsController.activeProfile.length > 0
                      ? qsTr("Active profile: %1").arg(manager.settingsController.activeProfile)
                      : qsTr("No active profile selected")
                color: Theme.secondaryText
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            AppButton {
                variant: "secondary"
                enabled: profileBox.currentText.length > 0
                text: manager.settingsController
                      && manager.settingsController.activeProfile === profileBox.currentText
                      ? qsTr("Active")
                      : qsTr("Set Active")
                animationsEnabled: manager.animationsEnabled
                onClicked: {
                    if (manager.settingsController) {
                        manager.settingsController.activeProfile = profileBox.currentText
                    }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 10
            rowSpacing: 10

            AppButton {
                Layout.fillWidth: true
                variant: "primary"
                enabled: profileBox.currentText.length > 0
                text: qsTr("Load")
                animationsEnabled: manager.animationsEnabled
                onClicked: {
                    const report = manager.appController.profileCompatibility(profileBox.currentText)
                    if (report.valid) {
                        manager.pendingProfile = profileBox.currentText
                        manager.compatibilityReport = report
                        compatibilityDialog.open()
                    }
                }
            }

            AppButton {
                Layout.fillWidth: true
                variant: "secondary"
                text: qsTr("Refresh")
                animationsEnabled: manager.animationsEnabled
                onClicked: manager.refreshProfiles()
            }

            AppButton {
                Layout.fillWidth: true
                variant: "secondary"
                enabled: profileBox.currentText.length > 0 && profileNameField.text.length > 0
                text: qsTr("Rename")
                animationsEnabled: manager.animationsEnabled
                onClicked: {
                    const oldName = profileBox.currentText
                    const newName = manager.normalizedProfileName(profileNameField.text)
                    if (manager.appController.renameProfile(oldName, profileNameField.text)) {
                        if (manager.settingsController && manager.settingsController.activeProfile === oldName) {
                            manager.settingsController.activeProfile = newName
                        }
                        manager.refreshProfiles(newName)
                    }
                }
            }

            AppButton {
                Layout.fillWidth: true
                variant: "secondary"
                enabled: profileBox.currentText.length > 0
                text: qsTr("Delete")
                animationsEnabled: manager.animationsEnabled
                onClicked: {
                    manager.pendingDeleteProfile = profileBox.currentText
                    deleteDialog.open()
                }
            }

            AppButton {
                Layout.fillWidth: true
                variant: "secondary"
                text: qsTr("Import")
                animationsEnabled: manager.animationsEnabled
                onClicked: importDialog.open()
            }

            AppButton {
                Layout.fillWidth: true
                variant: "secondary"
                enabled: profileBox.currentText.length > 0
                text: qsTr("Export")
                animationsEnabled: manager.animationsEnabled
                onClicked: exportDialog.open()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.divider
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Profiles directory: %1").arg(manager.appController ? manager.appController.profilesDirectory : "")
            color: Theme.secondaryText
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        Item {
            Layout.fillHeight: true
        }
    }

    Connections {
        target: manager.appController

        function onProfilesChanged() {
            manager.refreshProfiles()
        }
    }

    Component.onCompleted: refreshProfiles(
        manager.settingsController ? manager.settingsController.activeProfile : "")
}
