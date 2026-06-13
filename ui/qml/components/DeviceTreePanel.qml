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
    readonly property int activeFilter: treeModel ? treeModel.deviceFilter : 1
    readonly property string activeFilterLabel: activeFilter === 0 ? qsTr("All") : qsTr("RGB")

    signal deviceSelected(int deviceIndex)
    signal zoneSelected(int deviceIndex, int zoneIndex)

    function applyFilter(filterIndex) {
        if (panel.treeModel) {
            panel.treeModel.deviceFilter = filterIndex
        }
        filterPopup.close()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Device Tree")
                    color: Theme.primaryText
                    font.pixelSize: 15
                    font.bold: true
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Pick a device or zone")
                    color: Theme.secondaryText
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }

            Button {
                id: filterButton

                Layout.alignment: Qt.AlignRight
                implicitWidth: filterButtonContent.implicitWidth + 28
                implicitHeight: 36
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
                    radius: 12
                    color: filterButton.down
                           ? Theme.hover
                           : (filterButton.hovered || filterPopup.opened ? Theme.elevated : Theme.inputBg)
                    border.color: filterButton.hovered || filterPopup.opened ? Theme.selectionBorder : Theme.border
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

        Popup {
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
                radius: 16
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
                    Layout.fillWidth: true
                    implicitWidth: 180
                    implicitHeight: 40
                    hoverEnabled: true
                    text: qsTr("All devices")
                    onClicked: panel.applyFilter(0)

                    background: Rectangle {
                        radius: 10
                        color: parent.down
                               ? Theme.hover
                               : (parent.hovered ? Theme.elevated : "transparent")

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
                    Layout.fillWidth: true
                    implicitWidth: 180
                    implicitHeight: 40
                    hoverEnabled: true
                    text: qsTr("RGB controllers")
                    onClicked: panel.applyFilter(1)

                    background: Rectangle {
                        radius: 10
                        color: parent.down
                               ? Theme.hover
                               : (parent.hovered ? Theme.elevated : "transparent")

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
                readonly property color zoneSwatchColor: zoneEffectColorHex || Theme.accent

                implicitWidth: tree.width
                implicitHeight: deviceNode ? 48 : 40
                leftPadding: 10
                rightPadding: 12
                topPadding: 6
                bottomPadding: 6
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

                ToolTip.text: model.description || ""
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
                    radius: 12
                    scale: treeDelegate.down ? 0.985 : 1
                    color: treeDelegate.selectedNode
                           ? Theme.selectionBg
                           : (treeDelegate.hovered ? Theme.hover : "transparent")
                    border.color: treeDelegate.selectedNode ? Theme.selectionBorder : "transparent"
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
                    spacing: 10

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
                                color: treeDelegate.selectedNode ? Theme.selectionText : Theme.secondaryText
                                animationsEnabled: panel.animationsEnabled
                                strokeWidth: 2
                            }
                        }
                    }

                    Item {
                        visible: treeDelegate.zoneNode
                        Layout.preferredWidth: 28
                        Layout.fillHeight: true
                        Layout.alignment: Qt.AlignVCenter

                        Rectangle {
                            x: 12
                            y: 0
                            width: 1
                            height: parent.height
                            color: Theme.treeLine
                        }

                        Rectangle {
                            x: 12
                            anchors.verticalCenter: parent.verticalCenter
                            width: 12
                            height: 1
                            color: Theme.treeLine
                        }
                    }

                    Item {
                        visible: treeDelegate.deviceNode
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                        Layout.alignment: Qt.AlignVCenter

                        NavIcon {
                            anchors.centerIn: parent
                            width: 20
                            height: 20
                            name: "motherboard"
                            color: treeDelegate.selectedNode ? Theme.selectionText : Theme.secondaryText
                            animationsEnabled: panel.animationsEnabled
                            strokeWidth: 1.8
                        }
                    }

                    Item {
                        visible: treeDelegate.zoneNode
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 26
                        Layout.alignment: Qt.AlignVCenter

                        Rectangle {
                            anchors.fill: parent
                            visible: !treeDelegate.zoneEffectAnimated
                            radius: 8
                            color: treeDelegate.zoneSwatchColor
                            border.color: treeDelegate.selectedNode
                                          ? Theme.selectionBorder
                                          : Qt.darker(treeDelegate.zoneSwatchColor, 1.15)
                            border.width: treeDelegate.selectedNode ? 2 : 1
                        }

                        Rectangle {
                            anchors.fill: parent
                            visible: treeDelegate.zoneEffectAnimated
                                     && (treeDelegate.zoneEffectType === 1 || treeDelegate.zoneEffectType === 3)
                            radius: 8
                            border.color: treeDelegate.selectedNode ? Theme.selectionBorder : Theme.border
                            border.width: treeDelegate.selectedNode ? 2 : 1
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

                        Rectangle {
                            anchors.fill: parent
                            visible: treeDelegate.zoneEffectAnimated
                                     && treeDelegate.zoneEffectType !== 1
                                     && treeDelegate.zoneEffectType !== 3
                            radius: 8
                            color: treeDelegate.zoneSwatchColor
                            opacity: 0.82
                            border.color: treeDelegate.selectedNode
                                          ? Theme.selectionBorder
                                          : Qt.darker(treeDelegate.zoneSwatchColor, 1.15)
                            border.width: treeDelegate.selectedNode ? 2 : 1
                        }

                        Rectangle {
                            visible: treeDelegate.zoneEffectAnimated
                            anchors.centerIn: parent
                            width: 8
                            height: 8
                            radius: 4
                            color: "#FFFFFF"
                            opacity: 0.86
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: model.displayName || ""
                            color: treeDelegate.selectedNode ? Theme.selectionText : Theme.primaryText
                            font.pixelSize: treeDelegate.deviceNode ? 13 : 12
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
                            visible: (model.description || "").length > 0
                            text: model.description || ""
                            color: treeDelegate.selectedNode ? Theme.selectionSubText : Theme.secondaryText
                            font.pixelSize: 10
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
                        font.pixelSize: 10
                        font.bold: true
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Rectangle {
                        visible: treeDelegate.zoneNode && treeDelegate.zoneEffectAnimated
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: Math.min(122, effectBadgeLabel.implicitWidth + 32)
                        Layout.preferredHeight: 24
                        radius: 10
                        color: treeDelegate.selectedNode ? Qt.rgba(1, 1, 1, 0.14) : Theme.inputBg
                        border.color: treeDelegate.selectedNode ? Theme.selectionBorder : Theme.border
                        border.width: 1

                        RowLayout {
                            id: effectBadgeContent

                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 6

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
                                color: treeDelegate.selectedNode ? Theme.selectionText : Theme.primaryText
                                font.pixelSize: 10
                                font.bold: true
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
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
                        radius: 14
                        color: Theme.surface
                        border.color: Theme.border
                        border.width: 1
                    }

                    contentItem: ColumnLayout {
                        spacing: 2

                        ItemDelegate {
                            visible: treeDelegate.deviceNode && !model.isRgbController
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
                                radius: 10
                                color: parent.down ? Theme.hover : (parent.hovered ? Theme.elevated : "transparent")

                                Behavior on color {
                                    ColorAnimation {
                                        duration: panel.animationDuration
                                    }
                                }
                            }

                            contentItem: Label {
                                text: qsTr("Register as RGB controller")
                                color: parent.enabled ? Theme.primaryText : Theme.secondaryText
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 10
                                rightPadding: 10
                            }
                        }

                        ItemDelegate {
                            visible: treeDelegate.deviceNode && model.isRgbController
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
                                radius: 10
                                color: parent.down ? Theme.hover : (parent.hovered ? Theme.elevated : "transparent")

                                Behavior on color {
                                    ColorAnimation {
                                        duration: panel.animationDuration
                                    }
                                }
                            }

                            contentItem: Label {
                                text: qsTr("Remove from RGB controllers")
                                color: parent.enabled ? Theme.primaryText : Theme.secondaryText
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 10
                                rightPadding: 10
                            }
                        }

                        ItemDelegate {
                            visible: treeDelegate.deviceNode && model.hasRgbControllerOverride
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
                                radius: 10
                                color: parent.down ? Theme.hover : (parent.hovered ? Theme.elevated : "transparent")

                                Behavior on color {
                                    ColorAnimation {
                                        duration: panel.animationDuration
                                    }
                                }
                            }

                            contentItem: Label {
                                text: qsTr("Reset RGB controller override")
                                color: parent.enabled ? Theme.primaryText : Theme.secondaryText
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
