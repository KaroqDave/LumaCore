import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LumaCore

Dialog {
    id: dialog

    title: qsTr("Choose Color")
    modal: true
    anchors.centerIn: parent
    padding: 24

    property color selectedColor: Theme.defaultColor
    property color draftColor: selectedColor
    property bool animationsEnabled: true

    property real hue: 220
    property real saturation: 0.7
    property real value: 0.85
    property bool syncing: false

    standardButtons: Dialog.NoButton

    function colorFromHsv(h, s, v) {
        return Qt.hsva(h / 360.0, s, v, 1.0)
    }

    function syncHsvFromColor(color) {
        syncing = true
        hue = color.hsvHue >= 0 ? color.hsvHue : 0
        saturation = color.hsvSaturation
        value = color.hsvValue
        draftColor = color
        hexField.text = dialog.colorToHex(color)
        syncing = false
    }

    function updateDraftFromHsv() {
        if (syncing) {
            return
        }
        draftColor = colorFromHsv(hue, saturation, value)
        hexField.text = dialog.colorToHex(draftColor)
    }

    function colorToHex(color) {
        const red = Math.round(color.r * 255).toString(16).padStart(2, "0")
        const green = Math.round(color.g * 255).toString(16).padStart(2, "0")
        const blue = Math.round(color.b * 255).toString(16).padStart(2, "0")
        return ("#" + red + green + blue).toUpperCase()
    }

    function parseHex(text) {
        const normalized = text.trim().replace(/^#/, "")
        if (!/^[0-9A-Fa-f]{6}$/.test(normalized)) {
            return null
        }
        return Qt.rgba(
            parseInt(normalized.slice(0, 2), 16) / 255,
            parseInt(normalized.slice(2, 4), 16) / 255,
            parseInt(normalized.slice(4, 6), 16) / 255,
            1.0)
    }

    onAboutToShow: syncHsvFromColor(selectedColor)

    onAccepted: selectedColor = draftColor

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

    contentItem: ColumnLayout {
        spacing: 14
        width: 360

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 180

            Rectangle {
                id: svField

                anchors.fill: parent
                radius: 12
                color: dialog.colorFromHsv(dialog.hue, 1, 1)
                border.color: Theme.border
                border.width: 1
                clip: true

                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius

                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "#FFFFFF" }
                        GradientStop { position: 1.0; color: "#00FFFFFF" }
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius

                    gradient: Gradient {
                        orientation: Gradient.Vertical
                        GradientStop { position: 0.0; color: "#00000000" }
                        GradientStop { position: 1.0; color: "#FF000000" }
                    }
                }

                Rectangle {
                    x: Math.max(0, Math.min(parent.width - width, dialog.saturation * parent.width - width / 2))
                    y: Math.max(0, Math.min(parent.height - height, (1.0 - dialog.value) * parent.height - height / 2))
                    width: 16
                    height: 16
                    radius: 8
                    color: "transparent"
                    border.color: "#FFFFFF"
                    border.width: 2
                }

                MouseArea {
                    anchors.fill: parent
                    onPositionChanged: function(mouse) {
                        if (!pressed) {
                            return
                        }
                        dialog.saturation = Math.max(0, Math.min(1, mouse.x / svField.width))
                        dialog.value = Math.max(0, Math.min(1, 1.0 - mouse.y / svField.height))
                        dialog.updateDraftFromHsv()
                    }
                    onClicked: function(mouse) {
                        dialog.saturation = Math.max(0, Math.min(1, mouse.x / svField.width))
                        dialog.value = Math.max(0, Math.min(1, 1.0 - mouse.y / svField.height))
                        dialog.updateDraftFromHsv()
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 18

            Rectangle {
                id: hueBar

                anchors.fill: parent
                radius: 9
                border.color: Theme.border
                border.width: 1
                clip: true

                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.00; color: "#FF0000" }
                    GradientStop { position: 0.17; color: "#FFFF00" }
                    GradientStop { position: 0.33; color: "#00FF00" }
                    GradientStop { position: 0.50; color: "#00FFFF" }
                    GradientStop { position: 0.67; color: "#0000FF" }
                    GradientStop { position: 0.83; color: "#FF00FF" }
                    GradientStop { position: 1.00; color: "#FF0000" }
                }

                Rectangle {
                    x: Math.max(0, Math.min(parent.width - width, dialog.hue / 360.0 * parent.width - width / 2))
                    anchors.verticalCenter: parent.verticalCenter
                    width: 14
                    height: parent.height + 4
                    radius: 7
                    color: "transparent"
                    border.color: "#FFFFFF"
                    border.width: 2
                }

                MouseArea {
                    anchors.fill: parent
                    onPositionChanged: function(mouse) {
                        if (!pressed) {
                            return
                        }
                        dialog.hue = Math.max(0, Math.min(360, mouse.x / hueBar.width * 360))
                        dialog.updateDraftFromHsv()
                    }
                    onClicked: function(mouse) {
                        dialog.hue = Math.max(0, Math.min(360, mouse.x / hueBar.width * 360))
                        dialog.updateDraftFromHsv()
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                text: qsTr("Hex")
                color: Theme.secondaryText
                font.pixelSize: 12
                font.bold: true
            }

            TextField {
                id: hexField

                Layout.fillWidth: true
                font.family: "monospace"
                font.pixelSize: 12
                selectByMouse: true
                maximumLength: 7
                placeholderText: "#1E54D6"
                onEditingFinished: {
                    const parsed = dialog.parseHex(text)
                    if (parsed) {
                        dialog.syncHsvFromColor(parsed)
                    } else {
                        text = dialog.colorToHex(dialog.draftColor)
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                text: qsTr("Color")
                color: Theme.secondaryText
                font.pixelSize: 12
                font.bold: true
            }

            Rectangle {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                radius: 8
                color: dialog.draftColor
                border.color: Theme.border
                border.width: 1
            }

            Label {
                Layout.fillWidth: true
                text: dialog.colorToHex(dialog.draftColor)
                color: Theme.primaryText
                font.family: "monospace"
                font.pixelSize: 12
            }
        }
    }

    footer: Item {
        implicitWidth: footerRow.implicitWidth
        implicitHeight: footerRow.implicitHeight + 8

        RowLayout {
            id: footerRow

            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 10

            AppButton {
                Layout.preferredWidth: 120
                variant: "secondary"
                text: qsTr("Cancel")
                animationsEnabled: dialog.animationsEnabled
                onClicked: dialog.reject()
            }

            AppButton {
                Layout.preferredWidth: 120
                variant: "primary"
                text: qsTr("OK")
                animationsEnabled: dialog.animationsEnabled
                onClicked: {
                    dialog.draftColor = dialog.colorFromHsv(dialog.hue, dialog.saturation, dialog.value)
                    dialog.accept()
                }
            }
        }
    }
}
