import QtQuick
import QtQuick.Controls
import LumaCore

Rectangle {
    id: chip

    property var controller
    property bool animationsEnabled: true
    property bool dryRunEnabled: false
    property string backendName: qsTr("Backend")
    property bool daemonConnected: controller ? controller.daemonConnected : false
    property string daemonState: controller ? controller.daemonState : qsTr("Disconnected")
    readonly property bool recoveryBusy: controller ? controller.daemonRecoveryBusy : false
    readonly property color stateColor: recoveryBusy
        ? Theme.accent
        : (daemonConnected ? Theme.success : Theme.warning)

    signal clicked()

    radius: 999
    color: chipMouse.containsMouse || chipMouse.pressed ? Theme.elevated : Theme.pillFill
    border.color: chip.dryRunEnabled ? Theme.warning : Theme.pillBorder
    border.width: chip.dryRunEnabled ? 1.5 : 1
    implicitWidth: chipRow.implicitWidth + 18
    implicitHeight: 26

    Behavior on color {
        ColorAnimation {
            duration: chip.animationsEnabled ? 120 : 0
        }
    }

    Row {
        id: chipRow

        anchors.centerIn: parent
        spacing: 6

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 8
            height: 8
            radius: 4
            color: chip.stateColor
        }

        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: chip.backendName
            color: Theme.pillText
            font.pixelSize: 11
            font.bold: true
        }

        Rectangle {
            visible: chip.dryRunEnabled
            anchors.verticalCenter: parent.verticalCenter
            width: dryRunLabel.implicitWidth + 10
            height: 18
            radius: 999
            color: Theme.warning
            opacity: 0.18
            border.color: Theme.warning
            border.width: 1

            Label {
                id: dryRunLabel

                anchors.centerIn: parent
                text: qsTr("Dry-run")
                color: Theme.warning
                font.pixelSize: 9
                font.bold: true
            }
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: daemonStateLabel.implicitWidth + 10
            height: 18
            radius: 999
            color: chip.stateColor
            opacity: 0.18
            border.color: chip.stateColor
            border.width: 1

            Label {
                id: daemonStateLabel

                anchors.centerIn: parent
                text: chip.daemonState
                color: chip.stateColor
                font.pixelSize: 9
                font.bold: true
            }
        }
    }

    MouseArea {
        id: chipMouse

        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: chip.clicked()
    }

    ToolTip.visible: chipMouse.containsMouse
    ToolTip.text: chip.dryRunEnabled
        ? qsTr("Dry-run active. %1 — click for backend details").arg(chip.daemonState)
        : qsTr("%1 — click for backend details").arg(chip.daemonState)
}
