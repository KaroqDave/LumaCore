pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import LumaCore

Dialog {
    id: dialog

    title: qsTr("Active Backend")
    modal: true
    anchors.centerIn: parent
    padding: 24

    property var controller
    property bool animationsEnabled: true
    readonly property bool recoveryBusy: controller ? controller.daemonRecoveryBusy : false
    readonly property string setupStatusLevel: controller ? controller.setupStatusLevel : "warning"
    readonly property color setupStatusColor: setupStatusLevel === "error"
        ? Theme.error
        : (setupStatusLevel === "warning"
           ? Theme.warning
           : (setupStatusLevel === "ready" ? Theme.success : Theme.accent))
    readonly property color setupStatusBackground: setupStatusLevel === "error"
        ? Theme.errorBg
        : (setupStatusLevel === "warning" ? Theme.warningBg : Theme.inputBg)

    standardButtons: Dialog.NoButton

    FileDialog {
        id: diagnosticsDialog

        title: qsTr("Export LumaCore Diagnostics")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [qsTr("LumaCore diagnostics (*.json)")]
        onAccepted: dialog.controller.exportDiagnostics(selectedFile)
    }

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
        implicitWidth: footerButtons.implicitWidth
        implicitHeight: footerButtons.implicitHeight + 8

        GridLayout {
            id: footerButtons

            anchors.horizontalCenter: parent.horizontalCenter
            columns: 3
            columnSpacing: 10
            rowSpacing: 10

            AppButton {
                visible: dialog.controller && !dialog.controller.daemonConnected
                enabled: dialog.controller
                    && dialog.controller.daemonState !== qsTr("Refreshing devices")
                Layout.preferredWidth: 130
                variant: "secondary"
                text: qsTr("Retry now")
                animationsEnabled: dialog.animationsEnabled
                onClicked: dialog.controller.retryDaemonConnection()
            }

            AppButton {
                enabled: dialog.controller
                    && dialog.controller.daemonConnected
                    && !dialog.recoveryBusy
                Layout.preferredWidth: 130
                variant: "secondary"
                text: qsTr("Rescan")
                animationsEnabled: dialog.animationsEnabled
                onClicked: dialog.controller.rescanDaemonDevices()
            }

            AppButton {
                enabled: dialog.controller
                Layout.preferredWidth: 130
                variant: "secondary"
                text: qsTr("Export")
                animationsEnabled: dialog.animationsEnabled
                onClicked: diagnosticsDialog.open()
            }

            AppButton {
                enabled: dialog.controller
                Layout.preferredWidth: 130
                variant: "secondary"
                text: qsTr("Copy Summary")
                animationsEnabled: dialog.animationsEnabled
                onClicked: dialog.controller.copyDiagnosticsSummary()
            }

            AppButton {
                id: closeButton

                Layout.preferredWidth: 130
                variant: "primary"
                text: qsTr("Close")
                animationsEnabled: dialog.animationsEnabled
                onClicked: dialog.close()
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 16
        width: 460

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Label {
                Layout.fillWidth: true
                text: dialog.controller ? dialog.controller.backendDisplayName : qsTr("Unknown backend")
                color: Theme.primaryText
                font.pixelSize: 22
                font.bold: true
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                text: dialog.controller
                      ? qsTr("ID: %1").arg(dialog.controller.backendId)
                      : ""
                color: Theme.secondaryText
                font.pixelSize: 12
                font.family: "monospace"
            }
        }

        Label {
            Layout.fillWidth: true
            text: dialog.controller ? dialog.controller.backendDescription : ""
            color: Theme.primaryText
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            lineHeight: 1.35
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: setupStatusColumn.implicitHeight + 20
            radius: 12
            color: dialog.setupStatusBackground
            border.color: dialog.setupStatusColor
            border.width: 1

            ColumnLayout {
                id: setupStatusColumn

                anchors.fill: parent
                anchors.margins: 10
                spacing: 4

                Label {
                    Layout.fillWidth: true
                    text: dialog.controller ? dialog.controller.setupStatusSummary : qsTr("Backend status unavailable")
                    color: Theme.primaryText
                    font.pixelSize: 13
                    font.bold: true
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    text: dialog.controller ? dialog.controller.setupStatusDetail : ""
                    color: Theme.primaryText
                    opacity: 0.9
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    text: dialog.controller ? dialog.controller.setupStatusAction : ""
                    color: Theme.secondaryText
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: qsTr("Capabilities")
                color: Theme.primaryText
                font.pixelSize: 13
                font.bold: true
            }

            Flow {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: dialog.controller && dialog.controller.backendCapabilitiesText.length > 0
                           ? dialog.controller.backendCapabilitiesText.split(", ")
                           : []

                    delegate: Rectangle {
                        id: capabilityPill

                        required property string modelData

                        radius: 999
                        color: Theme.elevated
                        border.color: Theme.border
                        implicitWidth: capLabel.implicitWidth + 16
                        implicitHeight: 24

                        Label {
                            id: capLabel

                            anchors.centerIn: parent
                            text: capabilityPill.modelData
                            color: Theme.primaryText
                            font.pixelSize: 11
                            font.bold: true
                        }
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
                    text: qsTr("Loaded devices")
                    color: Theme.secondaryText
                    font.pixelSize: 11
                }

                Label {
                    Layout.fillWidth: true
                    text: dialog.controller ? String(dialog.controller.backendDeviceCount) : "0"
                    color: Theme.primaryText
                    font.pixelSize: 18
                    font.bold: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Dry-run mode")
                    color: Theme.secondaryText
                    font.pixelSize: 11
                }

                Label {
                    Layout.fillWidth: true
                    text: dialog.controller && dialog.controller.dryRunEnabled
                          ? qsTr("Enabled")
                          : qsTr("Disabled")
                    color: dialog.controller && dialog.controller.dryRunEnabled ? Theme.warning : Theme.success
                    font.pixelSize: 18
                    font.bold: true
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
                    text: qsTr("Daemon state")
                    color: Theme.secondaryText
                    font.pixelSize: 11
                }

                Label {
                    Layout.fillWidth: true
                    text: dialog.controller ? dialog.controller.daemonState : qsTr("Unknown")
                    color: dialog.recoveryBusy
                           ? Theme.accent
                           : (dialog.controller && dialog.controller.daemonConnected ? Theme.success : Theme.warning)
                    font.pixelSize: 18
                    font.bold: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Daemon version")
                    color: Theme.secondaryText
                    font.pixelSize: 11
                }

                Label {
                    Layout.fillWidth: true
                    text: dialog.controller && dialog.controller.daemonVersion.length > 0
                          ? dialog.controller.daemonVersion
                          : qsTr("Unavailable")
                    color: Theme.primaryText
                    font.pixelSize: 18
                    font.bold: true
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: noteLabel.implicitHeight + 16
            radius: 12
            color: Theme.inputBg
            border.color: Theme.border

            Label {
                id: noteLabel

                anchors.fill: parent
                anchors.margins: 12
                text: dialog.controller
                      ? qsTr("Socket: %1%2")
                            .arg(dialog.controller.daemonSocketPath)
                            .arg(dialog.controller.daemonLastError.length > 0
                                 ? qsTr("\nLast error: %1").arg(dialog.controller.daemonLastError)
                                 : "")
                      : ""
                color: Theme.secondaryText
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }
    }
}
