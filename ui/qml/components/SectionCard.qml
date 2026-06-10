import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: card

    property string title: ""
    property string subtitle: ""
    property color surfaceColor: "#1E242A"
    property color borderColor: "#343C44"
    property color primaryTextColor: "#F2F5F8"
    property color secondaryTextColor: "#AEB8C2"
    property bool animationsEnabled: true
    property int cardPadding: 16
    default property alias content: body.data

    color: surfaceColor
    radius: 16
    border.color: borderColor
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
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            visible: card.title.length > 0 || card.subtitle.length > 0
            spacing: 4

            Label {
                Layout.fillWidth: true
                text: card.title
                visible: card.title.length > 0
                color: card.primaryTextColor
                font.pixelSize: 15
                font.bold: true
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: card.subtitle
                visible: card.subtitle.length > 0
                color: card.secondaryTextColor
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
    }
}
