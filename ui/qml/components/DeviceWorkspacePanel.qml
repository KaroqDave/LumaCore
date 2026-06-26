// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LumaCore

Item {
    id: panel

    property var appController
    property int selectedDeviceIndex: -1
    property int selectedZoneIndex: -1
    property string selectedZoneName: ""
    property bool animationsEnabled: true
    readonly property bool hasZone: appController && selectedDeviceIndex >= 0 && selectedZoneIndex >= 0
    readonly property bool selectedDeviceWritable: hasZone ? appController.deviceWritable(selectedDeviceIndex) : false
    readonly property int selectedEffectType: hasZone ? appController.zoneEffectType(selectedDeviceIndex, selectedZoneIndex) : 0
    readonly property int selectedBrightness: hasZone ? appController.zoneEffectBrightness(selectedDeviceIndex, selectedZoneIndex) : 100
    readonly property real selectedSpeed: hasZone ? appController.zoneEffectSpeed(selectedDeviceIndex, selectedZoneIndex) : 1.0
    readonly property string selectedColorHex: hasZone ? appController.zoneColorHex(selectedDeviceIndex, selectedZoneIndex) : ""
    readonly property string selectedDeviceName: hasZone ? appController.deviceName(selectedDeviceIndex) : ""
    readonly property int selectedLedCount: hasZone ? appController.zoneLedCount(selectedDeviceIndex, selectedZoneIndex) : 0
    readonly property bool wideLayout: width >= 700
    readonly property int columnWidth: wideLayout ? Math.max(0, Math.floor((width - 26) / 3)) : width

    implicitHeight: content.implicitHeight

    function effectLabel(effectType) {
        switch (effectType) {
        case 1:
            return qsTr("Rainbow")
        case 2:
            return qsTr("Breathing")
        case 3:
            return qsTr("Cycle")
        default:
            return qsTr("Static")
        }
    }

    function recentLogLines() {
        if (!appController || appController.logText.length === 0) {
            return []
        }
        const lines = appController.logText.split("\n").filter(function(line) {
            return line.trim().length > 0
        })
        return lines.slice(Math.max(0, lines.length - 5)).reverse()
    }

    function presetSupported(effectType) {
        return hasZone
            && selectedDeviceWritable
            && appController.zoneSupportsEffect(selectedDeviceIndex, selectedZoneIndex, effectType)
    }

    function applyPreset(effectType, colorValue, speedValue, brightnessValue) {
        if (!presetSupported(effectType)) {
            return
        }
        appController.applyEffect(
            selectedDeviceIndex,
            selectedZoneIndex,
            effectType,
            colorValue,
            speedValue,
            brightnessValue)
    }

    function supportsLabel(effectType) {
        return hasZone && appController.zoneSupportsEffect(selectedDeviceIndex, selectedZoneIndex, effectType)
            ? qsTr("Yes")
            : qsTr("No")
    }

    GridLayout {
        id: content

        anchors.fill: parent
        columns: panel.wideLayout ? 5 : 1
        columnSpacing: 12
        rowSpacing: 12

        ColumnLayout {
            Layout.preferredWidth: panel.columnWidth
            Layout.fillWidth: !panel.wideLayout
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: qsTr("Selected Zone")
                color: Theme.primaryText
                font.pixelSize: 13
                font.bold: true
                elide: Text.ElideRight
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                rowSpacing: 5

                Label {
                    text: qsTr("Device")
                    color: Theme.secondaryText
                    font.pixelSize: 10
                }

                Label {
                    Layout.fillWidth: true
                    text: panel.hasZone ? panel.selectedDeviceName : qsTr("No zone selected")
                    color: Theme.primaryText
                    font.pixelSize: 11
                    font.bold: panel.hasZone
                    elide: Text.ElideRight
                }

                Label {
                    text: qsTr("Zone")
                    color: Theme.secondaryText
                    font.pixelSize: 10
                }

                Label {
                    Layout.fillWidth: true
                    text: panel.hasZone ? panel.selectedZoneName : qsTr("-")
                    color: Theme.primaryText
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }

                Label {
                    text: qsTr("LEDs")
                    color: Theme.secondaryText
                    font.pixelSize: 10
                }

                Label {
                    text: panel.hasZone ? panel.selectedLedCount : qsTr("-")
                    color: Theme.primaryText
                    font.pixelSize: 11
                    font.family: "monospace"
                }

                Label {
                    text: qsTr("Effect")
                    color: Theme.secondaryText
                    font.pixelSize: 10
                }

                Label {
                    text: panel.hasZone ? panel.effectLabel(panel.selectedEffectType) : qsTr("-")
                    color: Theme.primaryText
                    font.pixelSize: 11
                }

                Label {
                    text: qsTr("Color")
                    color: Theme.secondaryText
                    font.pixelSize: 10
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Rectangle {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        radius: 5
                        color: panel.hasZone && panel.selectedColorHex.length > 0 ? panel.selectedColorHex : Theme.inputBg
                        border.color: Theme.border
                    }

                    Label {
                        Layout.fillWidth: true
                        text: panel.hasZone ? panel.selectedColorHex : qsTr("-")
                        color: Theme.primaryText
                        font.pixelSize: 11
                        font.family: "monospace"
                        elide: Text.ElideRight
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: panel.hasZone
                spacing: 6

                StatusBadge {
                    text: panel.selectedDeviceWritable ? qsTr("Writable") : qsTr("Read-only")
                    colorValue: panel.selectedDeviceWritable ? Theme.success : Theme.warning
                }

                StatusBadge {
                    text: qsTr("%1%").arg(panel.selectedBrightness)
                    colorValue: Theme.accent
                }

                StatusBadge {
                    text: panel.selectedEffectType === 0 ? qsTr("Fixed") : qsTr("%1x").arg(panel.selectedSpeed.toFixed(1))
                    colorValue: Theme.pillText
                }
            }
        }

        Rectangle {
            visible: panel.wideLayout
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            color: Theme.divider
        }

        ColumnLayout {
            Layout.preferredWidth: panel.columnWidth
            Layout.fillWidth: !panel.wideLayout
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: qsTr("Quick Presets")
                color: Theme.primaryText
                font.pixelSize: 13
                font.bold: true
                elide: Text.ElideRight
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 8
                rowSpacing: 8

                AppButton {
                    Layout.fillWidth: true
                    text: qsTr("Soft White")
                    compact: true
                    enabled: panel.presetSupported(0)
                    animationsEnabled: panel.animationsEnabled
                    onClicked: panel.applyPreset(0, Qt.rgba(1.0, 0.953, 0.839, 1.0), 1.0, 55)
                }

                AppButton {
                    Layout.fillWidth: true
                    text: qsTr("Low Blue")
                    compact: true
                    enabled: panel.presetSupported(0)
                    animationsEnabled: panel.animationsEnabled
                    onClicked: panel.applyPreset(0, Qt.rgba(0.118, 0.329, 0.839, 1.0), 1.0, 28)
                }

                AppButton {
                    Layout.fillWidth: true
                    text: qsTr("Rainbow")
                    compact: true
                    enabled: panel.presetSupported(1)
                    animationsEnabled: panel.animationsEnabled
                    onClicked: panel.applyPreset(1, Qt.rgba(1.0, 1.0, 1.0, 1.0), 1.0, 70)
                }

                AppButton {
                    Layout.fillWidth: true
                    text: qsTr("Breathing")
                    compact: true
                    enabled: panel.presetSupported(2)
                    animationsEnabled: panel.animationsEnabled
                    onClicked: panel.applyPreset(2, Qt.rgba(0.486, 0.361, 1.0, 1.0), 1.2, 50)
                }

                AppButton {
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                    text: qsTr("All Off Selected Zone")
                    compact: true
                    variant: "secondary"
                    enabled: panel.presetSupported(0)
                    animationsEnabled: panel.animationsEnabled
                    onClicked: panel.applyPreset(0, Qt.rgba(0, 0, 0, 1.0), 1.0, 0)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                StatusBadge {
                    text: qsTr("Static %1").arg(panel.supportsLabel(0))
                    colorValue: panel.presetSupported(0) ? Theme.success : Theme.secondaryText
                }

                StatusBadge {
                    text: qsTr("Rainbow %1").arg(panel.supportsLabel(1))
                    colorValue: panel.presetSupported(1) ? Theme.success : Theme.secondaryText
                }
            }
        }

        Rectangle {
            visible: panel.wideLayout
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            color: Theme.divider
        }

        ColumnLayout {
            Layout.preferredWidth: panel.columnWidth
            Layout.fillWidth: !panel.wideLayout
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: qsTr("Recent Activity")
                color: Theme.primaryText
                font.pixelSize: 13
                font.bold: true
                elide: Text.ElideRight
            }

            Repeater {
                model: panel.recentLogLines()

                delegate: Label {
                    required property string modelData

                    Layout.fillWidth: true
                    text: modelData
                    color: Theme.secondaryText
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }

            Label {
                Layout.fillWidth: true
                visible: panel.recentLogLines().length === 0
                text: qsTr("No activity yet.")
                color: Theme.secondaryText
                font.pixelSize: 10
                elide: Text.ElideRight
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }

    component StatusBadge: Rectangle {
        id: badge

        property string text: ""
        property color colorValue: Theme.pillText

        Layout.preferredWidth: Math.min(130, badgeText.implicitWidth + 18)
        Layout.preferredHeight: 22
        radius: 8
        color: Qt.rgba(colorValue.r, colorValue.g, colorValue.b, Theme.dark ? 0.16 : 0.11)
        border.color: colorValue
        border.width: 1

        Label {
            id: badgeText

            anchors.centerIn: parent
            width: parent.width - 10
            text: badge.text
            color: badge.colorValue
            font.pixelSize: 9
            font.bold: true
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
