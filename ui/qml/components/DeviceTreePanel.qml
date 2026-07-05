// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LumaCore

Item {
    id: panel

    property var treeModel
    property var appController
    property int selectedDeviceIndex: -1
    property int selectedZoneIndex: -1
    property bool animationsEnabled: true
    readonly property int animationDuration: animationsEnabled ? Theme.animationDuration : 0
    readonly property int activeFilter: treeModel ? treeModel.deviceFilter : 0
    readonly property string activeFilterLabel: activeFilter === 0 ? qsTr("All") : qsTr("RGB")

    signal deviceSelected(int deviceIndex)
    signal zoneSelected(int deviceIndex, int zoneIndex)
    signal zoneEditRequested(int deviceIndex, int zoneIndex)

    function applyFilter(filterIndex) {
        if (panel.treeModel) {
            panel.treeModel.deviceFilter = filterIndex
        }
        filterPopup.close()
    }

    function badgeColor(level) {
        if (level === "ready") {
            return Theme.success
        }
        if (level === "warning") {
            return Theme.warning
        }
        return Theme.secondaryText
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 7

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Device Tree")
                    color: Theme.primaryText
                    font.pixelSize: 14
                    font.bold: true
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Pick a device or zone")
                    color: Theme.secondaryText
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }

            Button {
                id: filterButton

                Layout.alignment: Qt.AlignRight
                implicitWidth: filterButtonContent.implicitWidth + 28
                implicitHeight: 30
                hoverEnabled: true
                text: panel.activeFilterLabel

                onClicked: filterPopup.open()

                contentItem: RowLayout {
                    id: filterButtonContent

                    spacing: 8

                    NavIcon {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        Layout.alignment: Qt.AlignVCenter
                        name: "filter"
                        color: Theme.secondaryText
                        animationsEnabled: panel.animationsEnabled
                        strokeWidth: 1.8
                    }

                    Label {
                        Layout.alignment: Qt.AlignVCenter
                        text: panel.activeFilterLabel
                        color: Theme.primaryText
                        font.pixelSize: 12
                        font.bold: true
                    }
                }

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: filterButton.down
                           ? Theme.hover
                           : (filterButton.hovered || filterPopup.opened ? Theme.elevated : Theme.sunken)
                    border.color: filterButton.hovered || filterPopup.opened ? Theme.accentSoftBorder : Theme.border
                    border.width: 1

                    Behavior on color {
                        ColorAnimation {
                            duration: panel.animationDuration
                        }
                    }

                    Behavior on border.color {
                        ColorAnimation {
                            duration: panel.animationDuration
                        }
                    }
                }
            }
        }

        QtObject {
            property Popup popup: Popup {
                id: filterPopup

            parent: panel
            x: Math.max(0, filterButton.x + filterButton.width - width)
            y: filterButton.y + filterButton.height + 8
            padding: 8
            modal: false
            focus: true
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

            enter: Transition {
                NumberAnimation {
                    property: "opacity"
                    from: 0
                    to: 1
                    duration: panel.animationDuration
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    property: "scale"
                    from: 0.96
                    to: 1
                    duration: panel.animationDuration
                    easing.type: Easing.OutCubic
                }
            }

            exit: Transition {
                NumberAnimation {
                    property: "opacity"
                    to: 0
                    duration: panel.animationsEnabled ? 100 : 0
                    easing.type: Easing.OutCubic
                }
            }

            background: Rectangle {
                radius: Theme.radiusMedium
                color: Theme.surface
                border.color: Theme.border
                border.width: 1
            }

            contentItem: ColumnLayout {
                id: filterPopupContent

                spacing: 4

                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    Layout.topMargin: 8
                    Layout.bottomMargin: 4
                    text: qsTr("Filter")
                    color: Theme.primaryText
                    font.pixelSize: 13
                    font.bold: true
                }

                ItemDelegate {
                    id: allDevicesFilter

                    Layout.fillWidth: true
                    implicitWidth: 180
                    implicitHeight: 34
                    hoverEnabled: true
                    text: qsTr("All devices")
                    onClicked: panel.applyFilter(0)

                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: allDevicesFilter.down
                               ? Theme.hover
                               : (allDevicesFilter.hovered ? Theme.elevated : "transparent")

                        Behavior on color {
                            ColorAnimation {
                                duration: panel.animationDuration
                            }
                        }
                    }

                    contentItem: RowLayout {
                        spacing: 10

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("All devices")
                            color: Theme.primaryText
                            font.pixelSize: 12
                        }

                        NavIcon {
                            visible: panel.activeFilter === 0
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            name: "check"
                            color: Theme.accent
                            animationsEnabled: panel.animationsEnabled
                            strokeWidth: 2.2
                        }
                    }
                }

                ItemDelegate {
                    id: rgbControllersFilter

                    Layout.fillWidth: true
                    implicitWidth: 180
                    implicitHeight: 34
                    hoverEnabled: true
                    text: qsTr("RGB controllers")
                    onClicked: panel.applyFilter(1)

                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: rgbControllersFilter.down
                               ? Theme.hover
                               : (rgbControllersFilter.hovered ? Theme.elevated : "transparent")

                        Behavior on color {
                            ColorAnimation {
                                duration: panel.animationDuration
                            }
                        }
                    }

                    contentItem: RowLayout {
                        spacing: 10

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("RGB controllers")
                            color: Theme.primaryText
                            font.pixelSize: 12
                        }

                        NavIcon {
                            visible: panel.activeFilter === 1
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            name: "check"
                            color: Theme.accent
                            animationsEnabled: panel.animationsEnabled
                            strokeWidth: 2.2
                        }
                    }
                }
            }
            }
        }

        TreeView {
            id: tree

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: panel.treeModel
            boundsBehavior: Flickable.StopAtBounds
            columnWidthProvider: function() { return width }

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: TreeViewDelegate {
                id: treeDelegate

                readonly property bool deviceNode: model.isDevice === true
                readonly property bool zoneNode: model.isZone === true
                readonly property int sourceDeviceIndex: typeof model.deviceIndex === "number" ? model.deviceIndex : -1
                readonly property int sourceZoneIndex: typeof model.zoneIndex === "number" ? model.zoneIndex : -1
                readonly property bool selectedNode: zoneNode
                                                 ? panel.selectedDeviceIndex === sourceDeviceIndex && panel.selectedZoneIndex === sourceZoneIndex
                                                 : panel.selectedDeviceIndex === sourceDeviceIndex && panel.selectedZoneIndex < 0
                readonly property bool zoneEffectAnimated: model.zoneEffectAnimated === true
                readonly property int zoneEffectType: typeof model.zoneEffectType === "number" ? model.zoneEffectType : 0
                readonly property string zoneColorHex: model.zoneColorHex || ""
                readonly property string zoneEffectColorHex: model.zoneEffectColorHex || zoneColorHex
                readonly property string zoneEffectName: model.zoneEffectName || ""
                readonly property string zoneEffectLabel: zoneEffectName.length > 0 ? qsTr("%1 active").arg(zoneEffectName) : ""
                // Animated zones bind the live frame color (streamed through
                // ZoneColorHexRole at frame rate) so the swatch shows the
                // running effect; static zones show the configured color.
                readonly property color zoneSwatchColor: (zoneEffectAnimated && zoneColorHex.length > 0
                                                          ? zoneColorHex
                                                          : zoneEffectColorHex) || Theme.accent

                implicitWidth: tree.width
                implicitHeight: deviceNode ? 40 : 33
                leftPadding: 8
                rightPadding: 10
                topPadding: 4
                bottomPadding: 4
                text: model.displayName || ""

                onClicked: {
                    if (deviceNode) {
                        tree.toggleExpanded(row)
                        panel.deviceSelected(sourceDeviceIndex)
                    } else if (zoneNode) {
                        panel.zoneSelected(sourceDeviceIndex, sourceZoneIndex)
                    }
                }

                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: {
                        if (!treeDelegate.deviceNode) {
                            return
                        }

                        panel.deviceSelected(treeDelegate.sourceDeviceIndex)
                        deviceContextPopup.open()
                    }
                }

                ToolTip.text: (model.discoverySupportStatus || "").length > 0
                              ? qsTr("%1\n%2").arg(model.description || "").arg(model.discoverySupportStatus)
                              : (model.description || "")
                ToolTip.visible: hovered && ToolTip.text.length > 0

                // Chevron lives in contentItem so it aligns with the icon + text block.
                indicator: Item {
                    implicitWidth: 0
                    implicitHeight: 0
                }

                background: Rectangle {
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    anchors.topMargin: 1
                    anchors.bottomMargin: 1
                    radius: Theme.radiusSmall
                    scale: treeDelegate.down ? 0.985 : 1
                    color: treeDelegate.selectedNode
                           ? Theme.accentSoft
                           : (treeDelegate.hovered ? Theme.hover : "transparent")
                    border.color: treeDelegate.selectedNode ? Theme.accentSoftBorder : "transparent"
                    border.width: 1

                    Behavior on color {
                        ColorAnimation {
                            duration: panel.animationDuration
                            easing.type: Easing.OutCubic
                        }
                    }

                    Behavior on border.color {
                        ColorAnimation {
                            duration: panel.animationDuration
                            easing.type: Easing.OutCubic
                        }
                    }

                    Behavior on scale {
                        NumberAnimation {
                            duration: panel.animationsEnabled ? 90 : 0
                            easing.type: Easing.OutCubic
                        }
                    }
                }

                contentItem: RowLayout {
                    spacing: 8

                    Item {
                        visible: treeDelegate.deviceNode && treeDelegate.hasChildren
                        Layout.preferredWidth: visible ? 16 : 0
                        Layout.preferredHeight: 16
                        Layout.alignment: Qt.AlignVCenter

                        Item {
                            anchors.centerIn: parent
                            width: 12
                            height: 12
                            rotation: treeDelegate.expanded ? 90 : 0

                            Behavior on rotation {
                                NumberAnimation {
                                    duration: panel.animationsEnabled ? 160 : 0
                                    easing.type: Easing.OutCubic
                                }
                            }

                            NavIcon {
                                anchors.fill: parent
                                name: "chevron"
                                color: treeDelegate.selectedNode ? Theme.pillText : Theme.secondaryText
                                animationsEnabled: panel.animationsEnabled
                                strokeWidth: 2
                            }
                        }
                    }

                    Item {
                        visible: treeDelegate.zoneNode
                        Layout.preferredWidth: 22
                        Layout.fillHeight: true
                        Layout.alignment: Qt.AlignVCenter

                        Rectangle {
                            x: 10
                            y: 0
                            width: 1
                            height: parent.height
                            color: Theme.treeLine
                            opacity: 0.8
                        }

                        Rectangle {
                            x: 10
                            anchors.verticalCenter: parent.verticalCenter
                            width: 12
                            height: 1
                            color: Theme.treeLine
                            opacity: 0.8
                        }
                    }

                    Item {
                        visible: treeDelegate.deviceNode
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        Layout.alignment: Qt.AlignVCenter

                        NavIcon {
                            anchors.centerIn: parent
                            width: 18
                            height: 18
                            name: "motherboard"
                            color: treeDelegate.selectedNode ? Theme.pillText : Theme.secondaryText
                            animationsEnabled: panel.animationsEnabled
                            strokeWidth: 1.8
                        }
                    }

                    Item {
                        visible: treeDelegate.zoneNode
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                        Layout.alignment: Qt.AlignVCenter

                        Rectangle {
                            anchors.fill: parent
                            visible: !treeDelegate.zoneEffectAnimated
                            radius: 7
                            color: treeDelegate.zoneSwatchColor
                            border.color: treeDelegate.selectedNode
                                          ? Theme.accentSoftBorder
                                          : Qt.darker(treeDelegate.zoneSwatchColor, 1.15)
                            border.width: treeDelegate.selectedNode ? 2 : 1
                        }

                        Rectangle {
                            anchors.fill: parent
                            visible: treeDelegate.zoneEffectAnimated
                                     && (treeDelegate.zoneEffectType === 1 || treeDelegate.zoneEffectType === 3)
                            radius: 7
                            border.color: treeDelegate.selectedNode ? Theme.accentSoftBorder : Theme.border
                            border.width: treeDelegate.selectedNode ? 2 : 1
                            gradient: RainbowGradient {}
                        }

                        Rectangle {
                            anchors.fill: parent
                            // Overlays the rainbow gradient too: while frames
                            // stream, the live color cycles here; the gradient
                            // stays as the idle backdrop underneath.
                            visible: treeDelegate.zoneEffectAnimated
                            radius: 7
                            color: treeDelegate.zoneSwatchColor
                            opacity: 0.82
                            border.color: treeDelegate.selectedNode
                                          ? Theme.accentSoftBorder
                                          : Qt.darker(treeDelegate.zoneSwatchColor, 1.15)
                            border.width: treeDelegate.selectedNode ? 2 : 1
                        }

                        Rectangle {
                            visible: treeDelegate.zoneEffectAnimated
                            anchors.centerIn: parent
                            width: 6
                            height: 6
                            radius: 3
                            color: "#FFFFFF"
                            opacity: 0.86
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 1

                        Label {
                            Layout.fillWidth: true
                            text: treeDelegate.model.displayName || ""
                            color: treeDelegate.selectedNode ? Theme.pillText : Theme.primaryText
                            font.pixelSize: treeDelegate.deviceNode ? 12 : 11
                            font.bold: treeDelegate.deviceNode || treeDelegate.selectedNode
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter

                            Behavior on color {
                                ColorAnimation {
                                    duration: panel.animationDuration
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: (treeDelegate.model.description || "").length > 0
                            text: treeDelegate.model.description || ""
                            color: treeDelegate.selectedNode ? Theme.pillText : Theme.secondaryText
                            opacity: treeDelegate.selectedNode ? 0.8 : 1.0
                            font.pixelSize: 9
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter

                            Behavior on color {
                                ColorAnimation {
                                    duration: panel.animationDuration
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }
                    }

                    Label {
                        visible: treeDelegate.zoneNode && !treeDelegate.zoneEffectAnimated
                        text: treeDelegate.zoneColorHex
                        color: Theme.secondaryText
                        font.family: "monospace"
                        font.pixelSize: 9
                        font.bold: true
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Rectangle {
                        visible: treeDelegate.deviceNode && (treeDelegate.model.deviceBadgeText || "").length > 0
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: Math.min(88, deviceBadgeLabel.implicitWidth + 16)
                        Layout.preferredHeight: 20
                        radius: Theme.radiusSmall
                        color: Qt.rgba(
                            panel.badgeColor(treeDelegate.model.deviceBadgeLevel).r,
                            panel.badgeColor(treeDelegate.model.deviceBadgeLevel).g,
                            panel.badgeColor(treeDelegate.model.deviceBadgeLevel).b,
                            Theme.dark ? 0.16 : 0.12)
                        border.color: panel.badgeColor(treeDelegate.model.deviceBadgeLevel)
                        border.width: 1

                        Label {
                            id: deviceBadgeLabel

                            anchors.centerIn: parent
                            text: treeDelegate.model.deviceBadgeText || ""
                            color: panel.badgeColor(treeDelegate.model.deviceBadgeLevel)
                            font.pixelSize: 9
                            font.bold: true
                            elide: Text.ElideRight
                        }
                    }

                    Rectangle {
                        visible: treeDelegate.zoneNode && treeDelegate.zoneEffectAnimated
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: Math.min(104, effectBadgeLabel.implicitWidth + 26)
                        Layout.preferredHeight: 20
                        radius: Theme.radiusSmall
                        color: treeDelegate.selectedNode ? Theme.surface : Theme.inputBg
                        border.color: treeDelegate.selectedNode ? Theme.accentSoftBorder : Theme.border
                        border.width: 1

                        RowLayout {
                            id: effectBadgeContent

                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            spacing: 5

                            Rectangle {
                                Layout.preferredWidth: 6
                                Layout.preferredHeight: 6
                                Layout.alignment: Qt.AlignVCenter
                                radius: 3
                                color: treeDelegate.zoneEffectType === 1 || treeDelegate.zoneEffectType === 3
                                       ? Theme.accent
                                       : treeDelegate.zoneSwatchColor
                            }

                            Label {
                                id: effectBadgeLabel

                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                text: treeDelegate.zoneEffectLabel
                                color: treeDelegate.selectedNode ? Theme.pillText : Theme.primaryText
                                font.pixelSize: 9
                                font.bold: true
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }

                    Item {
                        visible: treeDelegate.zoneNode
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: 58
                        Layout.preferredHeight: 28

                        AppButton {
                            anchors.fill: parent
                            text: qsTr("Edit")
                            compact: true
                            controlHeight: 28
                            font.pixelSize: 11
                            leftPadding: 10
                            rightPadding: 10
                            variant: treeDelegate.selectedNode ? "primary" : "secondary"
                            enabled: panel.appController !== null && panel.appController !== undefined
                            animationsEnabled: panel.animationsEnabled
                            onClicked: {
                                panel.zoneSelected(treeDelegate.sourceDeviceIndex, treeDelegate.sourceZoneIndex)
                                panel.zoneEditRequested(treeDelegate.sourceDeviceIndex, treeDelegate.sourceZoneIndex)
                            }
                        }
                    }
                }

                Popup {
                    id: deviceContextPopup

                    parent: treeDelegate
                    x: Math.max(8, treeDelegate.width - width - 12)
                    y: Math.min(treeDelegate.height + 2, tree.height - treeDelegate.y - height)
                    padding: 6
                    modal: false
                    focus: true
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

                    enter: Transition {
                        NumberAnimation {
                            property: "opacity"
                            from: 0
                            to: 1
                            duration: panel.animationsEnabled ? 100 : 0
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            property: "scale"
                            from: 0.97
                            to: 1
                            duration: panel.animationsEnabled ? 100 : 0
                            easing.type: Easing.OutCubic
                        }
                    }

                    exit: Transition {
                        NumberAnimation {
                            property: "opacity"
                            to: 0
                            duration: panel.animationsEnabled ? 80 : 0
                        }
                    }

                    background: Rectangle {
                        radius: Theme.radiusMedium
                        color: Theme.surface
                        border.color: Theme.border
                        border.width: 1
                    }

                    contentItem: ColumnLayout {
                        spacing: 2

                        ItemDelegate {
                            id: registerControllerAction

                            visible: treeDelegate.deviceNode && !treeDelegate.model.isRgbController
                            Layout.fillWidth: true
                            implicitWidth: 228
                            implicitHeight: visible ? 38 : 0
                            hoverEnabled: true
                            enabled: panel.appController !== null && panel.appController !== undefined
                            onClicked: {
                                panel.appController.markDeviceRgbController(treeDelegate.sourceDeviceIndex)
                                deviceContextPopup.close()
                            }

                            background: Rectangle {
                                radius: Theme.radiusSmall
                                color: registerControllerAction.down
                                       ? Theme.hover
                                       : (registerControllerAction.hovered ? Theme.elevated : "transparent")

                                Behavior on color {
                                    ColorAnimation {
                                        duration: panel.animationDuration
                                    }
                                }
                            }

                            contentItem: Label {
                                text: qsTr("Register as RGB controller")
                                color: registerControllerAction.enabled ? Theme.primaryText : Theme.secondaryText
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 10
                                rightPadding: 10
                            }
                        }

                        ItemDelegate {
                            id: removeControllerAction

                            visible: treeDelegate.deviceNode && treeDelegate.model.isRgbController
                            Layout.fillWidth: true
                            implicitWidth: 228
                            implicitHeight: visible ? 38 : 0
                            hoverEnabled: true
                            enabled: panel.appController !== null && panel.appController !== undefined
                            onClicked: {
                                panel.appController.removeDeviceRgbController(treeDelegate.sourceDeviceIndex)
                                deviceContextPopup.close()
                            }

                            background: Rectangle {
                                radius: Theme.radiusSmall
                                color: removeControllerAction.down
                                       ? Theme.hover
                                       : (removeControllerAction.hovered ? Theme.elevated : "transparent")

                                Behavior on color {
                                    ColorAnimation {
                                        duration: panel.animationDuration
                                    }
                                }
                            }

                            contentItem: Label {
                                text: qsTr("Remove from RGB controllers")
                                color: removeControllerAction.enabled ? Theme.primaryText : Theme.secondaryText
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 10
                                rightPadding: 10
                            }
                        }

                        ItemDelegate {
                            id: resetControllerAction

                            visible: treeDelegate.deviceNode && treeDelegate.model.hasRgbControllerOverride
                            Layout.fillWidth: true
                            implicitWidth: 228
                            implicitHeight: visible ? 38 : 0
                            hoverEnabled: true
                            enabled: panel.appController !== null && panel.appController !== undefined
                            onClicked: {
                                panel.appController.resetDeviceRgbControllerOverride(treeDelegate.sourceDeviceIndex)
                                deviceContextPopup.close()
                            }

                            background: Rectangle {
                                radius: Theme.radiusSmall
                                color: resetControllerAction.down
                                       ? Theme.hover
                                       : (resetControllerAction.hovered ? Theme.elevated : "transparent")

                                Behavior on color {
                                    ColorAnimation {
                                        duration: panel.animationDuration
                                    }
                                }
                            }

                            contentItem: Label {
                                text: qsTr("Reset RGB controller override")
                                color: resetControllerAction.enabled ? Theme.primaryText : Theme.secondaryText
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 10
                                rightPadding: 10
                            }
                        }
                    }
                }
            }

            Component.onCompleted: expand(0)
        }
    }
}
