import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LumaCore

Rectangle {
    id: panel

    property var controller
    property bool animationsEnabled: true
    readonly property string level: controller ? controller.setupStatusLevel : "warning"
    readonly property bool ready: level === "ready"
    readonly property color statusColor: level === "error"
        ? Theme.error
        : (level === "warning" ? Theme.warning : (level === "ready" ? Theme.success : Theme.accent))
    readonly property color statusBackground: level === "error"
        ? Theme.errorBg
        : (level === "warning" ? Theme.warningBg : Theme.elevated)

    visible: controller && !ready
    Layout.fillWidth: true
    implicitHeight: visible ? content.implicitHeight + 24 : 0
    radius: 14
    color: statusBackground
    border.color: statusColor
    border.width: 1

    Behavior on color {
        ColorAnimation {
            duration: panel.animationsEnabled ? 140 : 0
        }
    }

    RowLayout {
        id: content

        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        NavIcon {
            Layout.preferredWidth: 22
            Layout.preferredHeight: 22
            Layout.alignment: Qt.AlignTop
            name: panel.level === "ready" ? "check" : "info"
            color: panel.statusColor
            animationsEnabled: panel.animationsEnabled
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Label {
                Layout.fillWidth: true
                text: panel.controller ? panel.controller.setupStatusSummary : ""
                color: Theme.primaryText
                font.pixelSize: 14
                font.bold: true
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                text: panel.controller ? panel.controller.setupStatusDetail : ""
                color: Theme.primaryText
                opacity: 0.9
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                text: panel.controller ? panel.controller.setupStatusAction : ""
                color: Theme.secondaryText
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignVCenter
            spacing: 8
            visible: panel.controller
                && panel.controller.daemonSocketPath.length > 0
                && panel.level !== "ready"

            AppButton {
                visible: panel.controller && !panel.controller.daemonConnected
                enabled: panel.controller && !panel.controller.daemonRecoveryBusy
                Layout.preferredWidth: 96
                variant: "secondary"
                text: qsTr("Retry")
                animationsEnabled: panel.animationsEnabled
                onClicked: panel.controller.retryDaemonConnection()
            }

            AppButton {
                visible: panel.controller && panel.controller.daemonConnected
                enabled: panel.controller && !panel.controller.daemonRecoveryBusy
                Layout.preferredWidth: 96
                variant: "secondary"
                text: qsTr("Rescan")
                animationsEnabled: panel.animationsEnabled
                onClicked: panel.controller.rescanDaemonDevices()
            }
        }
    }
}
