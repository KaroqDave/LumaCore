import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: manager

    property var appController
    property color borderColor: "#343C44"
    property color primaryTextColor: "#F2F5F8"
    property color secondaryTextColor: "#AEB8C2"

    function refreshProfiles() {
        if (!appController) {
            profileBox.model = []
            return
        }

        const previous = profileBox.currentText
        profileBox.model = appController.profileNames()
        const index = profileBox.find(previous)
        if (index >= 0) {
            profileBox.currentIndex = index
        }
    }

    implicitHeight: content.implicitHeight

    ColumnLayout {
        id: content

        anchors.fill: parent
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: qsTr("Save, restore, and organize mock RGB profiles.")
            color: manager.secondaryTextColor
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

            Button {
                text: qsTr("Save")
                onClicked: {
                    if (manager.appController.saveProfile(profileNameField.text)) {
                        manager.refreshProfiles()
                    }
                }
            }
        }

        ComboBox {
            id: profileBox

            Layout.fillWidth: true
            onActivated: profileNameField.text = currentText
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 10
            rowSpacing: 10

            Button {
                Layout.fillWidth: true
                enabled: profileBox.currentText.length > 0
                text: qsTr("Load")
                onClicked: manager.appController.loadProfile(profileBox.currentText)
            }

            Button {
                Layout.fillWidth: true
                text: qsTr("Refresh")
                onClicked: manager.refreshProfiles()
            }

            Button {
                Layout.fillWidth: true
                enabled: profileBox.currentText.length > 0 && profileNameField.text.length > 0
                text: qsTr("Rename")
                onClicked: {
                    if (manager.appController.renameProfile(profileBox.currentText, profileNameField.text)) {
                        manager.refreshProfiles()
                    }
                }
            }

            Button {
                Layout.fillWidth: true
                enabled: profileBox.currentText.length > 0
                text: qsTr("Delete")
                onClicked: {
                    if (manager.appController.deleteProfile(profileBox.currentText)) {
                        profileNameField.text = "default"
                        manager.refreshProfiles()
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: manager.borderColor
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Profiles directory: %1").arg(manager.appController ? manager.appController.profilesDirectory : "")
            color: manager.secondaryTextColor
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

    Component.onCompleted: refreshProfiles()
}
