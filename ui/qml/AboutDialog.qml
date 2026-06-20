import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LumaCore

Dialog {
    id: dialog

    title: qsTr("About LumaCore")
    modal: true
    anchors.centerIn: parent
    padding: 24

    property bool animationsEnabled: true
    property var controller

    standardButtons: Dialog.NoButton

    background: Rectangle {
        color: Theme.surface
        radius: 18
        border.color: Theme.border
        border.width: 1
    }

    header: Label {
        text: dialog.title
        color: Theme.primaryText
        font.pixelSize: 17
        font.bold: true
        padding: 18
        leftPadding: 24
        rightPadding: 24
        topPadding: 20
        bottomPadding: 0
    }

    footer: Item {
        implicitWidth: closeButton.implicitWidth
        implicitHeight: closeButton.implicitHeight + 8

        AppButton {
            id: closeButton

            anchors.horizontalCenter: parent.horizontalCenter
            width: 160
            variant: "primary"
            text: qsTr("Close")
            animationsEnabled: dialog.animationsEnabled
            onClicked: dialog.close()
        }
    }

    contentItem: ColumnLayout {
        spacing: 18
        width: 440

        RowLayout {
            Layout.fillWidth: true
            spacing: 18

            Rectangle {
                Layout.preferredWidth: 72
                Layout.preferredHeight: 72
                radius: 18
                color: Theme.elevated
                border.color: Theme.accent
                border.width: 1

                Image {
                    id: aboutIconImage

                    anchors.centerIn: parent
                    width: 58
                    height: 58
                    source: "qrc:///icons/lumacore-256.png"
                    sourceSize.width: 116
                    sourceSize.height: 116
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    visible: status === Image.Ready
                }

                Label {
                    anchors.centerIn: parent
                    visible: aboutIconImage.status !== Image.Ready
                    text: qsTr("LC")
                    color: Theme.primaryText
                    font.pixelSize: 24
                    font.bold: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    Layout.fillWidth: true
                    text: Qt.application.name.length > 0 ? Qt.application.name : qsTr("LumaCore")
                    color: Theme.primaryText
                    font.pixelSize: 26
                    font.bold: true
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    text: Qt.application.version.length > 0
                          ? qsTr("Version %1").arg(Qt.application.version)
                          : qsTr("Version unknown")
                    color: Theme.secondaryText
                    font.pixelSize: 14
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: Qt.platform.os === "windows"
                  ? qsTr("Windows Preview built with C++23 and Qt 6, licensed under GPL-2.0-or-later. This preview runs the existing daemon architecture with simulated mock devices only; Windows hardware discovery and RGB writes are not supported.")
                  : qsTr("Linux-first RGB control built with C++23 and Qt 6, licensed under GPL-2.0-or-later. This release keeps the GUI unprivileged, routes backend access through the LumaCore daemon, and exposes controller-aware effects for mock, discovery, and ASUS Aura HID devices.")
            color: Theme.primaryText
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            lineHeight: 1.35
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: infoPill.implicitHeight + 16
            radius: 999
            color: Theme.pillFill
            border.color: Theme.pillBorder

            Label {
                id: infoPill

                anchors.centerIn: parent
                text: Qt.platform.os === "windows"
                      ? qsTr("Daemon-backed - mock devices only")
                      : qsTr("Daemon-backed - session-confirmed ASUS writes")
                color: Theme.pillText
                font.pixelSize: 12
                font.bold: true
            }
        }

        Label {
            Layout.fillWidth: true
            text: Qt.platform.os === "windows"
                  ? qsTr("The Windows Preview is isolated from physical RGB hardware. Effects and profiles operate only on the simulated device.")
                  : qsTr("ASUS Aura HID support uses allowlisted, config-verified packets for static colors and native hardware modes. Unsupported controller parameters are disabled in the effect editor.")
            color: Theme.secondaryText
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            lineHeight: 1.3
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Profiles are stored in %1").arg(dialog.controller ? dialog.controller.profilesDirectory : "")
            color: Theme.secondaryText
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }
}
