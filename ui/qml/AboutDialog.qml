import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog

    title: qsTr("About LumaCore")
    modal: true
    anchors.centerIn: parent
    padding: 24
    standardButtons: Dialog.Close

    readonly property color surfaceColor: "#1E242A"
    readonly property color elevatedColor: "#252B32"
    readonly property color accentColor: "#42A5F5"
    readonly property color borderColor: "#343C44"
    readonly property color primaryTextColor: "#F2F5F8"
    readonly property color secondaryTextColor: "#AEB8C2"

    background: Rectangle {
        color: dialog.surfaceColor
        radius: 18
        border.color: dialog.borderColor
        border.width: 1
    }

    header: Label {
        text: dialog.title
        color: dialog.primaryTextColor
        font.pixelSize: 17
        font.bold: true
        padding: 18
        leftPadding: 24
        rightPadding: 24
        topPadding: 20
        bottomPadding: 0
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
                color: dialog.elevatedColor
                border.color: dialog.accentColor
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
                    color: dialog.primaryTextColor
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
                    color: dialog.primaryTextColor
                    font.pixelSize: 26
                    font.bold: true
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    text: Qt.application.version.length > 0
                          ? qsTr("Version %1").arg(Qt.application.version)
                          : qsTr("Version unknown")
                    color: dialog.secondaryTextColor
                    font.pixelSize: 14
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Linux-first, open-source RGB control built with C++23 and Qt 6. This release is mock-only: it simulates devices and profiles safely before any real hardware support.")
            color: dialog.primaryTextColor
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            lineHeight: 1.35
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: infoPill.implicitHeight + 16
            radius: 999
            color: "#24313B"
            border.color: "#344B5D"

            Label {
                id: infoPill

                anchors.centerIn: parent
                text: qsTr("Mock backend only — no hardware access")
                color: "#BFE3FF"
                font.pixelSize: 12
                font.bold: true
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Profiles are stored in %1").arg(appController.profilesDirectory)
            color: dialog.secondaryTextColor
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }
}
