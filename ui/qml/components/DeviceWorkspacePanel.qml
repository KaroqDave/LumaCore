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
    property bool selectedZoneMode: true
    property string selectedTargetName: ""
    property int selectedZoneRevision: 0
    property int activityRevision: 0
    readonly property bool hasZone: appController && selectedDeviceIndex >= 0 && selectedZoneIndex >= 0
    readonly property bool selectedDeviceWritable: selectedZoneRevision >= 0 && hasZone ? appController.deviceWritable(selectedDeviceIndex) : false
    readonly property int selectedEffectType: selectedZoneRevision >= 0 && hasZone ? appController.zoneEffectType(selectedDeviceIndex, selectedZoneIndex) : 0
    readonly property int selectedBrightness: selectedZoneRevision >= 0 && hasZone ? appController.zoneEffectBrightness(selectedDeviceIndex, selectedZoneIndex) : 100
    readonly property real selectedSpeed: selectedZoneRevision >= 0 && hasZone ? appController.zoneEffectSpeed(selectedDeviceIndex, selectedZoneIndex) : 1.0
    readonly property color selectedEffectColor: selectedZoneRevision >= 0 && hasZone ? appController.zoneEffectColor(selectedDeviceIndex, selectedZoneIndex) : Theme.defaultColor
    readonly property string selectedColorHex: selectedZoneRevision >= 0 && hasZone ? appController.zoneColorHex(selectedDeviceIndex, selectedZoneIndex) : ""
    readonly property string selectedDeviceName: selectedZoneRevision >= 0 && hasZone ? appController.deviceName(selectedDeviceIndex) : ""
    readonly property int selectedLedCount: selectedZoneRevision >= 0 && hasZone ? appController.zoneLedCount(selectedDeviceIndex, selectedZoneIndex) : 0
    readonly property int columnSpacing: 12
    readonly property int dividerWidth: 1
    readonly property int wideLayoutReservedWidth: (columnSpacing * 4) + (dividerWidth * 2)
    readonly property int wideLayoutAvailableWidth: Math.max(0, width - wideLayoutReservedWidth)
    readonly property bool wideLayout: width >= 760
    readonly property int primaryColumnWidth: wideLayout ? Math.max(180, Math.floor(wideLayoutAvailableWidth * 0.25)) : width
    readonly property int activityColumnWidth: wideLayout ? Math.min(300, Math.max(230, Math.floor(wideLayoutAvailableWidth * 0.31))) : width
    readonly property int presetColumnWidth: wideLayout ? Math.max(220, wideLayoutAvailableWidth - primaryColumnWidth - activityColumnWidth) : width
    readonly property bool groupTargetSelected: selectedTargetName.length > 0

    signal presetApplied(int effectType, color colorValue, real speedValue, int brightnessValue)

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
        activityRevision
        if (!appController || appController.logText.length === 0) {
            return []
        }
        const lines = appController.logText.split("\n").filter(function(line) {
            return line.trim().length > 0
        })
        return lines.slice(Math.max(0, lines.length - 5)).reverse()
    }

    function presetSupported(effectType) {
        if (!appController) {
            return false
        }
        if (selectedZoneMode) {
            return hasZone
                && selectedDeviceWritable
                && appController.zoneSupportsEffect(selectedDeviceIndex, selectedZoneIndex, effectType)
        }
        return appController.globalTargetSupportsEffect(selectedTargetName, effectType)
    }

    function effectStatusLabel(effectType) {
        switch (effectType) {
        case 1:
            return qsTr("Rainbow active")
        case 2:
            return qsTr("Breathing active")
        case 3:
            return qsTr("Cycle active")
        default:
            return qsTr("Fixed")
        }
    }

    function applyPreset(effectType, colorValue, speedValue, brightnessValue) {
        if (!presetSupported(effectType)) {
            return
        }
        const applied = selectedZoneMode
            ? appController.applyEffect(
                  selectedDeviceIndex,
                  selectedZoneIndex,
                  effectType,
                  colorValue,
                  speedValue,
                  brightnessValue)
            : (groupTargetSelected
               ? appController.applyEffectToDeviceGroup(
                     selectedTargetName,
                     effectType,
                     colorValue,
                     speedValue,
                     brightnessValue)
               : appController.applyEffectGlobally(
                     effectType,
                     colorValue,
                     speedValue,
                     brightnessValue))
        if (applied) {
            presetApplied(effectType, colorValue, speedValue, brightnessValue)
        }
    }

    function effectSupported(effectType) {
        return selectedZoneRevision >= 0 && presetSupported(effectType)
    }

    function allOffPresetLabel() {
        if (selectedZoneMode) {
            return qsTr("All Off Selected Zone")
        }
        return groupTargetSelected ? qsTr("All Off Group") : qsTr("All Off All Devices")
    }

    function refreshSelectedZone() {
        selectedZoneRevision += 1
    }

    GridLayout {
        id: content

        anchors.fill: parent
        columns: panel.wideLayout ? 5 : 1
        columnSpacing: panel.columnSpacing
        rowSpacing: 10

        ColumnLayout {
            Layout.preferredWidth: panel.primaryColumnWidth
            Layout.maximumWidth: panel.wideLayout ? panel.primaryColumnWidth : 16777215
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
                        color: panel.hasZone && panel.selectedEffectType !== 1 && panel.selectedEffectType !== 3
                               ? panel.selectedEffectColor
                               : Theme.sunken
                        border.color: Theme.border
                        clip: true

                        Rectangle {
                            anchors.fill: parent
                            visible: panel.hasZone && (panel.selectedEffectType === 1 || panel.selectedEffectType === 3)
                            radius: parent.radius
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: "#FF0000" }
                                GradientStop { position: 0.2; color: "#FFFF00" }
                                GradientStop { position: 0.4; color: "#00FF00" }
                                GradientStop { position: 0.6; color: "#00FFFF" }
                                GradientStop { position: 0.8; color: "#0000FF" }
                                GradientStop { position: 1.0; color: "#FF00FF" }
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: panel.hasZone
                              ? (panel.selectedEffectType === 0
                                 ? panel.selectedColorHex
                                 : panel.effectStatusLabel(panel.selectedEffectType))
                              : qsTr("-")
                        color: Theme.primaryText
                        font.pixelSize: 11
                        font.family: panel.hasZone && panel.selectedEffectType === 0 ? "monospace" : "Segoe UI"
                        font.bold: false
                        elide: Text.ElideRight
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: panel.hasZone
                spacing: 6

                StatusBadge {
                    visible: panel.selectedDeviceWritable
                    text: panel.selectedDeviceWritable ? qsTr("Writable") : qsTr("Read-only")
                    colorValue: panel.selectedDeviceWritable ? Theme.success : Theme.warning
                }

                StatusBadge {
                    visible: !panel.selectedDeviceWritable
                    text: qsTr("Read-only")
                    colorValue: Theme.warning
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
            Layout.preferredWidth: panel.dividerWidth
            Layout.maximumWidth: panel.dividerWidth
            color: Theme.divider
            opacity: 0.8
        }

        ColumnLayout {
            Layout.preferredWidth: panel.presetColumnWidth
            Layout.maximumWidth: panel.wideLayout ? panel.presetColumnWidth : 16777215
            Layout.fillWidth: !panel.wideLayout
            spacing: 8

            Flow {
                Layout.fillWidth: true
                visible: panel.hasZone
                spacing: 6

                StatusBadge {
                    visible: panel.effectSupported(0)
                    text: qsTr("Static")
                    colorValue: Theme.success
                }

                StatusBadge {
                    visible: panel.effectSupported(1)
                    text: qsTr("Rainbow")
                    colorValue: Theme.success
                }

                StatusBadge {
                    visible: panel.effectSupported(2)
                    text: qsTr("Breathing")
                    colorValue: Theme.success
                }

                StatusBadge {
                    visible: panel.effectSupported(3)
                    text: qsTr("Cycle")
                    colorValue: Theme.success
                }
            }

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
                    text: panel.allOffPresetLabel()
                    compact: true
                    variant: "secondary"
                    enabled: panel.presetSupported(0)
                    animationsEnabled: panel.animationsEnabled
                    onClicked: panel.applyPreset(0, Qt.rgba(0, 0, 0, 1.0), 1.0, 0)
                }
            }

        }

        Rectangle {
            visible: panel.wideLayout
            Layout.fillHeight: true
            Layout.preferredWidth: panel.dividerWidth
            Layout.maximumWidth: panel.dividerWidth
            color: Theme.divider
            opacity: 0.8
        }

        ColumnLayout {
            Layout.preferredWidth: panel.activityColumnWidth
            Layout.maximumWidth: panel.wideLayout ? panel.activityColumnWidth : 16777215
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

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 92
                Layout.preferredHeight: panel.wideLayout ? 118 : 132
                radius: 8
                color: Theme.sunken
                border.color: Theme.border
                border.width: 1
                clip: true

                ScrollView {
                    id: recentActivityScroll

                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    contentWidth: availableWidth

                    ColumnLayout {
                        width: recentActivityScroll.availableWidth
                        spacing: 5

                        Repeater {
                            model: panel.recentLogLines()

                            delegate: Label {
                                required property string modelData

                                Layout.fillWidth: true
                                width: recentActivityScroll.availableWidth
                                text: modelData
                                color: Theme.secondaryText
                                font.family: "monospace"
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                ToolTip.visible: activityMouse.containsMouse && truncated
                                ToolTip.text: text

                                MouseArea {
                                    id: activityMouse

                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.NoButton
                                }
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
                    }
                }
            }

        }
    }

    onSelectedDeviceIndexChanged: refreshSelectedZone()
    onSelectedZoneIndexChanged: refreshSelectedZone()
    onAppControllerChanged: refreshSelectedZone()

    Connections {
        target: panel.appController

        function onZoneDataChanged(deviceIndex, zoneIndex) {
            if (deviceIndex === panel.selectedDeviceIndex
                    && (zoneIndex < 0 || zoneIndex === panel.selectedZoneIndex)) {
                panel.refreshSelectedZone()
            }
        }

        function onBackendInfoChanged() {
            panel.refreshSelectedZone()
        }

        function onDaemonDevicesRefreshed() {
            panel.refreshSelectedZone()
        }

        function onLogTextChanged() {
            panel.activityRevision += 1
        }
    }

    component StatusBadge: Rectangle {
        id: badge

        property string text: ""
        property color colorValue: Theme.pillText

        width: implicitWidth
        height: implicitHeight
        implicitWidth: Math.min(130, badgeText.implicitWidth + 16)
        implicitHeight: 21
        Layout.preferredWidth: implicitWidth
        Layout.preferredHeight: implicitHeight
        radius: 8
        color: Qt.rgba(colorValue.r, colorValue.g, colorValue.b, Theme.dark ? 0.16 : 0.11)
        border.color: colorValue
        border.width: 1
        antialiasing: true

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
