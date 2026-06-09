import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: root

    width: 1280
    height: 820
    minimumWidth: 980
    minimumHeight: 680
    visible: true
    title: qsTr("LumaCore")
    color: "#15191D"

    property color selectedColor: "#4080FF"
    readonly property color surfaceColor: "#1E242A"
    readonly property color elevatedColor: "#252B32"
    readonly property color accentColor: "#42A5F5"
    readonly property color borderColor: "#343C44"
    readonly property color primaryTextColor: "#F2F5F8"
    readonly property color secondaryTextColor: "#AEB8C2"

    function colorToHex(value) {
        const red = Math.round(value.r * 255).toString(16).padStart(2, "0")
        const green = Math.round(value.g * 255).toString(16).padStart(2, "0")
        const blue = Math.round(value.b * 255).toString(16).padStart(2, "0")
        return ("#" + red + green + blue).toUpperCase()
    }

    component SectionCard: Rectangle {
        id: card

        property string title: ""
        property string subtitle: ""
        default property alias content: body.data

        color: root.surfaceColor
        radius: 18
        border.color: root.borderColor
        border.width: 1
        implicitHeight: body.implicitHeight + 36

        ColumnLayout {
            id: body

            anchors.fill: parent
            anchors.margins: 18
            spacing: 14

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    Layout.fillWidth: true
                    text: card.title
                    color: root.primaryTextColor
                    font.pixelSize: 17
                    font.bold: true
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    visible: card.subtitle.length > 0
                    text: card.subtitle
                    color: root.secondaryTextColor
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    component PillLabel: Rectangle {
        id: pill

        property string text: ""

        radius: 999
        color: "#24313B"
        border.color: "#344B5D"
        implicitWidth: label.implicitWidth + 20
        implicitHeight: 28

        Label {
            id: label

            anchors.centerIn: parent
            text: pill.text
            color: "#BFE3FF"
            font.pixelSize: 12
            font.bold: true
        }
    }

    ColorDialog {
        id: colorDialog

        title: qsTr("Choose Static Color")
        selectedColor: root.selectedColor
        onAccepted: root.selectedColor = selectedColor
    }

    AboutDialog {
        id: aboutDialog
    }

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: Math.max(availableWidth, 1120)
        contentHeight: contentColumn.implicitHeight + 48
        ScrollBar.vertical.policy: ScrollBar.AlwaysOn
        ScrollBar.vertical.interactive: true
        ScrollBar.horizontal.policy: ScrollBar.AsNeeded
        ScrollBar.horizontal.interactive: true

        ColumnLayout {
            id: contentColumn

            width: Math.max(root.width - 48, 1120)
            x: 24
            y: 24
            spacing: 22

            RowLayout {
                Layout.fillWidth: true
                spacing: 18

                Rectangle {
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 56
                    radius: 16
                    color: root.elevatedColor
                    border.color: root.accentColor
                    border.width: 1

                    Image {
                        id: headerIconImage

                        anchors.centerIn: parent
                        width: 46
                        height: 46
                        source: "qrc:///icons/lumacore-256.png"
                        sourceSize.width: 92
                        sourceSize.height: 92
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        visible: status === Image.Ready
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: headerIconImage.status !== Image.Ready
                        text: qsTr("LC")
                        color: root.primaryTextColor
                        font.pixelSize: 20
                        font.bold: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("LumaCore")
                        color: root.primaryTextColor
                        font.pixelSize: 34
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Modern mock-only RGB control. Build profiles and UI flows safely before real hardware support.")
                        color: root.secondaryTextColor
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                    }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 10

                    PillLabel {
                        text: qsTr("Mock backend only")
                    }

                    ToolButton {
                        id: aboutButton

                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        icon.name: "dialog-information"
                        icon.width: 22
                        icon.height: 22
                        ToolTip.text: qsTr("About LumaCore")
                        ToolTip.visible: hovered
                        onClicked: aboutDialog.open()

                        background: Rectangle {
                            radius: 12
                            color: aboutButton.down ? "#2A3540" : (aboutButton.hovered ? "#24313B" : root.elevatedColor)
                            border.color: aboutButton.hovered ? root.accentColor : root.borderColor
                            border.width: 1
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 54
                color: root.elevatedColor
                radius: 14
                border.color: root.borderColor

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 10
                        Layout.preferredHeight: 10
                        radius: 5
                        color: "#4CAF50"
                    }

                    Label {
                        Layout.fillWidth: true
                        text: appController.statusMessage
                        color: root.primaryTextColor
                        font.pixelSize: 14
                        font.bold: true
                        elide: Text.ElideRight
                        ToolTip.visible: statusMouse.containsMouse && truncated
                        ToolTip.text: text

                        MouseArea {
                            id: statusMouse

                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 660
                spacing: 22

                SectionCard {
                    Layout.preferredWidth: 340
                    Layout.minimumWidth: 300
                    Layout.fillHeight: true
                    title: qsTr("Devices")
                    subtitle: qsTr("Available mock RGB devices")

                    ListView {
                        id: deviceList

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 10
                        model: deviceModel
                        currentIndex: count > 0 ? 0 : -1
                        boundsBehavior: Flickable.StopAtBounds
                        onCurrentIndexChanged: {
                            zoneModel.deviceIndex = currentIndex
                            zoneList.currentIndex = zoneList.count > 0 ? 0 : -1
                        }

                        delegate: ItemDelegate {
                            id: deviceDelegate

                            required property int index
                            required property string deviceName
                            required property string vendor
                            required property string deviceType
                            required property int zoneCount

                            width: ListView.view.width
                            height: 92
                            padding: 0
                            highlighted: ListView.isCurrentItem
                            onClicked: deviceList.currentIndex = index

                            background: Rectangle {
                                radius: 14
                                color: deviceDelegate.highlighted ? "#2D79B8" : root.elevatedColor
                                border.color: deviceDelegate.highlighted ? "#67C1FF" : root.borderColor
                                border.width: 1
                            }

                            contentItem: ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 7

                                Label {
                                    Layout.fillWidth: true
                                    text: deviceDelegate.deviceName
                                    color: root.primaryTextColor
                                    font.pixelSize: 14
                                    font.bold: true
                                    elide: Text.ElideRight
                                    ToolTip.visible: deviceNameMouse.containsMouse && truncated
                                    ToolTip.text: text

                                    MouseArea {
                                        id: deviceNameMouse

                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.NoButton
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("%1 · %2").arg(deviceDelegate.vendor).arg(deviceDelegate.deviceType)
                                    color: deviceDelegate.highlighted ? "#D7EEFF" : root.secondaryTextColor
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("%1 zone(s)").arg(deviceDelegate.zoneCount)
                                    color: deviceDelegate.highlighted ? "#D7EEFF" : root.secondaryTextColor
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }
                }

                SectionCard {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 360
                    Layout.fillHeight: true
                    title: qsTr("Zones")
                    subtitle: qsTr("Select the target zone before applying a color")

                    ListView {
                        id: zoneList

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 10
                        model: zoneModel
                        currentIndex: count > 0 ? 0 : -1
                        boundsBehavior: Flickable.StopAtBounds

                        delegate: ItemDelegate {
                            id: zoneDelegate

                            required property int index
                            required property string zoneName
                            required property string zoneType
                            required property int ledCount
                            required property string zoneColorHex

                            width: ListView.view.width
                            height: 88
                            padding: 0
                            highlighted: ListView.isCurrentItem
                            onClicked: zoneList.currentIndex = index

                            background: Rectangle {
                                radius: 14
                                color: zoneDelegate.highlighted ? "#2D79B8" : root.elevatedColor
                                border.color: zoneDelegate.highlighted ? "#67C1FF" : root.borderColor
                                border.width: 1
                            }

                            contentItem: RowLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 14

                                Rectangle {
                                    Layout.preferredWidth: 42
                                    Layout.preferredHeight: 42
                                    radius: 12
                                    color: zoneDelegate.zoneColorHex
                                    border.color: "#FFFFFF"
                                    border.width: zoneDelegate.highlighted ? 2 : 1
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Label {
                                        Layout.fillWidth: true
                                        text: zoneDelegate.zoneName
                                        color: root.primaryTextColor
                                        font.pixelSize: 15
                                        font.bold: true
                                        elide: Text.ElideRight
                                        ToolTip.visible: zoneNameMouse.containsMouse && truncated
                                        ToolTip.text: text

                                        MouseArea {
                                            id: zoneNameMouse

                                            anchors.fill: parent
                                            hoverEnabled: true
                                            acceptedButtons: Qt.NoButton
                                        }
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("%1 · %2 LEDs").arg(zoneDelegate.zoneType).arg(zoneDelegate.ledCount)
                                        color: zoneDelegate.highlighted ? "#D7EEFF" : root.secondaryTextColor
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                }

                                Label {
                                    Layout.preferredWidth: 84
                                    text: zoneDelegate.zoneColorHex
                                    color: root.primaryTextColor
                                    font.family: "monospace"
                                    font.pixelSize: 13
                                    horizontalAlignment: Text.AlignRight
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.preferredWidth: 390
                    Layout.minimumWidth: 360
                    Layout.fillHeight: true
                    spacing: 22

                    SectionCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 300
                        title: qsTr("Static Color")
                        subtitle: qsTr("Pick a color and apply it to the selected zone")

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 82
                            radius: 16
                            color: root.selectedColor
                            border.color: "#FFFFFF"
                            border.width: 1
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Label {
                                Layout.fillWidth: true
                                text: root.colorToHex(root.selectedColor)
                                color: root.primaryTextColor
                                font.family: "monospace"
                                font.pixelSize: 22
                                font.bold: true
                            }

                            Button {
                                text: qsTr("Choose")
                                onClicked: colorDialog.open()
                            }
                        }

                        Button {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 42
                            text: qsTr("Apply to Selected Zone")
                            enabled: deviceList.currentIndex >= 0 && zoneList.currentIndex >= 0
                            onClicked: appController.applyStaticColor(deviceList.currentIndex, zoneList.currentIndex, root.selectedColor)
                        }
                    }

                    SectionCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 320
                        title: qsTr("Profiles")
                        subtitle: qsTr("Save and restore mock zone colors")

                        TextField {
                            id: profileNameField

                            Layout.fillWidth: true
                            Layout.preferredHeight: 42
                            text: "default"
                            placeholderText: qsTr("Profile name")
                            selectByMouse: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Button {
                                Layout.fillWidth: true
                                text: qsTr("Save")
                                onClicked: {
                                    if (appController.saveProfile(profileNameField.text)) {
                                        profileBox.model = appController.profileNames()
                                    }
                                }
                            }

                            Button {
                                Layout.fillWidth: true
                                text: qsTr("Refresh")
                                onClicked: profileBox.model = appController.profileNames()
                            }
                        }

                        ComboBox {
                            id: profileBox

                            Layout.fillWidth: true
                            Layout.preferredHeight: 42
                            model: appController.profileNames()
                        }

                        Button {
                            Layout.fillWidth: true
                            text: qsTr("Load Selected Profile")
                            enabled: profileBox.currentText.length > 0
                            onClicked: appController.loadProfile(profileBox.currentText)
                        }
                    }
                }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 210
                title: qsTr("Status / Log")
                subtitle: qsTr("Recent mock backend and profile activity")

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Profiles: %1").arg(appController.profilesDirectory)
                    color: root.secondaryTextColor
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                TextArea {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: appController.logText
                    readOnly: true
                    wrapMode: TextArea.Wrap
                    color: root.primaryTextColor
                    selectedTextColor: root.primaryTextColor
                    selectionColor: root.accentColor
                    font.family: "monospace"
                    font.pixelSize: 12

                    background: Rectangle {
                        color: "#171C21"
                        radius: 12
                        border.color: root.borderColor
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        zoneModel.deviceIndex = deviceList.currentIndex
    }
}
