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
    readonly property bool hasZone: appController && selectedDeviceIndex >= 0 && selectedZoneIndex >= 0
    // A single reactive token for the selected zone. It is -1 when no zone is selected and
    // otherwise carries selectedZoneRevision, which refreshSelectedZone() bumps on every
    // zone-data/backend/refresh event. Because its value changes on each bump, bindings that
    // read it re-evaluate the (non-NOTIFY) zone invokables below and pick up fresh data, while
    // `zoneTick >= 0` stays a genuine "has a valid zone" check rather than an always-true guard.
    readonly property int zoneTick: hasZone ? selectedZoneRevision : -1
    readonly property bool selectedDeviceWritable: zoneTick >= 0 ? appController.deviceWritable(selectedDeviceIndex) : false
    readonly property bool selectedDeviceRequiresConfirmation: zoneTick >= 0 ? appController.deviceRequiresConfirmation(selectedDeviceIndex) : false
    readonly property bool selectedDeviceWriteConfirmed: zoneTick >= 0 ? appController.deviceWriteConfirmed(selectedDeviceIndex) : false
    // A writable device that still needs per-session confirmation is not yet
    // arming real writes, so it is surfaced distinctly from a granted device.
    readonly property bool selectedDeviceNeedsConfirmation: appController
        && !appController.dryRunEnabled
        && selectedDeviceWritable
        && selectedDeviceRequiresConfirmation
        && !selectedDeviceWriteConfirmed
    readonly property int selectedEffectType: zoneTick >= 0 ? appController.zoneEffectType(selectedDeviceIndex, selectedZoneIndex) : 0
    readonly property int selectedBrightness: zoneTick >= 0 ? appController.zoneEffectBrightness(selectedDeviceIndex, selectedZoneIndex) : 100
    readonly property real selectedSpeed: zoneTick >= 0 ? appController.zoneEffectSpeed(selectedDeviceIndex, selectedZoneIndex) : 1.0
    readonly property color selectedEffectColor: zoneTick >= 0 ? appController.zoneEffectColor(selectedDeviceIndex, selectedZoneIndex) : Theme.defaultColor
    readonly property string selectedColorHex: zoneTick >= 0 ? appController.zoneColorHex(selectedDeviceIndex, selectedZoneIndex) : ""
    readonly property string selectedDeviceName: zoneTick >= 0 ? appController.deviceName(selectedDeviceIndex) : ""
    readonly property int selectedLedCount: zoneTick >= 0 ? appController.zoneLedCount(selectedDeviceIndex, selectedZoneIndex) : 0
    // All non-empty activity lines, newest first (the controller caps its log
    // at 500 entries). appController.logText has a NOTIFY, so this binding
    // refreshes on its own; it is cached so the model and the empty-state
    // check below do not each re-parse the log string.
    readonly property var recentLogLinesModel: {
        if (!appController || appController.logText.length === 0) {
            return []
        }
        return appController.logText.split("\n").filter(function(line) {
            return line.trim().length > 0
        }).reverse()
    }
    readonly property int columnSpacing: 12
    readonly property int dividerWidth: 1
    readonly property int wideLayoutReservedWidth: (columnSpacing * 4) + (dividerWidth * 2)
    readonly property int wideLayoutAvailableWidth: Math.max(0, width - wideLayoutReservedWidth)
    readonly property bool wideLayout: width >= 760
    readonly property int primaryColumnWidth: wideLayout ? Math.max(180, Math.floor(wideLayoutAvailableWidth * 0.25)) : width
    readonly property int activityColumnWidth: wideLayout ? Math.min(380, Math.max(260, Math.floor(wideLayoutAvailableWidth * 0.36))) : width
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
        return effectType === 0 ? qsTr("Fixed") : qsTr("%1 active").arg(effectLabel(effectType))
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

    // Whether the hardware can do this effect, independent of whether the device is currently
    // writable. The preset buttons gate on presetSupported() (which also requires a writable,
    // confirmed device); the capability badges use this so a read-only zone still advertises
    // which effects it supports rather than showing nothing.
    function effectCapable(effectType) {
        if (!appController) {
            return false
        }
        if (selectedZoneMode) {
            return zoneTick >= 0
                && appController.zoneSupportsEffect(selectedDeviceIndex, selectedZoneIndex, effectType)
        }
        // Touch selectedZoneRevision so the badge re-evaluates when the backend/target refreshes.
        return selectedZoneRevision >= 0
            && appController.globalTargetSupportsEffect(selectedTargetName, effectType)
    }

    function allOffPresetLabel() {
        if (selectedZoneMode) {
            // In zone mode the action targets the whole selected device
            // (allOffDevice), so the label must reflect device scope, not zone.
            return qsTr("All Off Device")
        }
        return groupTargetSelected ? qsTr("All Off Group") : qsTr("All Off All Devices")
    }

    function allOffSelectedTarget() {
        if (!appController) {
            return
        }
        if (selectedZoneMode) {
            appController.allOffDevice(selectedDeviceIndex)
            return
        }
        if (groupTargetSelected) {
            appController.allOffDeviceGroup(selectedTargetName)
            return
        }
        appController.allOffAllDevices()
    }

    function allOffSupported() {
        if (!appController) {
            return false
        }
        if (selectedZoneMode) {
            // Only enable the write control when the device can actually accept
            // writes; a read-only device would fail the all-off command.
            return hasZone && selectedDeviceIndex >= 0 && selectedDeviceWritable
        }
        return selectedZoneRevision >= 0
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
                            gradient: RainbowGradient {}
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
                    text: panel.selectedDeviceNeedsConfirmation
                          ? qsTr("Confirm")
                          : (panel.selectedDeviceWritable ? qsTr("Writable") : qsTr("Read-only"))
                    colorValue: panel.selectedDeviceWritable && !panel.selectedDeviceNeedsConfirmation
                          ? Theme.success
                          : Theme.warning
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
                    visible: panel.effectCapable(0)
                    text: qsTr("Static")
                    colorValue: Theme.success
                }

                StatusBadge {
                    visible: panel.effectCapable(1)
                    text: qsTr("Rainbow")
                    colorValue: Theme.success
                }

                StatusBadge {
                    visible: panel.effectCapable(2)
                    text: qsTr("Breathing")
                    colorValue: Theme.success
                }

                StatusBadge {
                    visible: panel.effectCapable(3)
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
                    enabled: panel.allOffSupported()
                    animationsEnabled: panel.animationsEnabled
                    onClicked: panel.allOffSelectedTarget()
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
                Layout.minimumHeight: 120
                Layout.preferredHeight: panel.wideLayout ? 156 : 168
                radius: Theme.radiusSmall
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
                            model: panel.recentLogLinesModel

                            delegate: Label {
                                required property string modelData

                                Layout.fillWidth: true
                                width: recentActivityScroll.availableWidth
                                text: modelData
                                color: Theme.secondaryText
                                font.family: "monospace"
                                font.pixelSize: 10
                                wrapMode: Text.WrapAnywhere
                                maximumLineCount: 2
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
                            visible: panel.recentLogLinesModel.length === 0
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
        radius: Theme.radiusSmall
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
