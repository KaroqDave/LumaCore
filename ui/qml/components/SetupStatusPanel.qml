// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LumaCore

Rectangle {
    id: panel

    property var controller
    property bool animationsEnabled: true
    property bool compact: false
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
    implicitHeight: visible ? content.implicitHeight + (compact ? 18 : 24) : 0
    radius: Theme.radiusLarge
    color: statusBackground
    border.color: Qt.alpha(statusColor, 0.45)
    border.width: 1

    Behavior on color {
        ColorAnimation {
            duration: panel.animationsEnabled ? 140 : 0
        }
    }

    RowLayout {
        id: content

        anchors.fill: parent
        anchors.margins: panel.compact ? 9 : 12
        spacing: panel.compact ? 9 : 12

        NavIcon {
            Layout.preferredWidth: panel.compact ? 18 : 22
            Layout.preferredHeight: panel.compact ? 18 : 22
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
                font.pixelSize: panel.compact ? 13 : 14
                font.bold: true
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                text: panel.controller ? panel.controller.setupStatusDetail : ""
                color: Theme.primaryText
                opacity: 0.86
                font.pixelSize: panel.compact ? 11 : 12
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
                compact: panel.compact
                onClicked: panel.controller.retryDaemonConnection()
            }

            AppButton {
                visible: panel.controller && panel.controller.daemonConnected
                enabled: panel.controller && !panel.controller.daemonRecoveryBusy
                Layout.preferredWidth: 96
                variant: "secondary"
                text: qsTr("Rescan")
                animationsEnabled: panel.animationsEnabled
                compact: panel.compact
                onClicked: panel.controller.rescanDaemonDevices()
            }
        }
    }
}
