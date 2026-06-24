import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LumaCore

Rectangle {
    id: card

    property string title: ""
    property string subtitle: ""
    property color surfaceColor: Theme.surface
    property bool animationsEnabled: true
    property bool compact: false
    property int cardPadding: compact ? 12 : 16
    default property alias content: body.data

    color: surfaceColor
    radius: 8
    border.color: Theme.border
    border.width: 1
    implicitHeight: body.implicitHeight + cardPadding * 2

    Behavior on color {
        ColorAnimation {
            duration: card.animationsEnabled ? 140 : 0
        }
    }

    ColumnLayout {
        id: body

        anchors.fill: parent
        anchors.margins: card.cardPadding
        spacing: card.compact ? 8 : 12

        ColumnLayout {
            Layout.fillWidth: true
            visible: card.title.length > 0 || card.subtitle.length > 0
            spacing: card.compact ? 2 : 4

            Label {
                Layout.fillWidth: true
                text: card.title
                visible: card.title.length > 0
                color: Theme.primaryText
                font.pixelSize: card.compact ? 14 : 15
                font.bold: true
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: card.subtitle
                visible: card.subtitle.length > 0
                color: Theme.secondaryText
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
    }
}
