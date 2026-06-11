import QtQuick
import QtQuick.Controls
import LumaCore

Button {
    id: control

    // "primary" | "secondary"
    property string variant: "secondary"
    property string iconName: ""
    property bool animationsEnabled: true
    readonly property bool primary: variant === "primary"
    readonly property int animationDuration: animationsEnabled ? 140 : 0

    implicitHeight: 40
    leftPadding: 16
    rightPadding: 16
    font.pixelSize: 13
    font.bold: true
    hoverEnabled: true

    contentItem: Item {
        implicitWidth: contentRow.implicitWidth
        implicitHeight: contentRow.implicitHeight

        Row {
            id: contentRow

            anchors.centerIn: parent
            spacing: 8

            NavIcon {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.iconName.length > 0
                width: 18
                height: 18
                name: control.iconName
                color: control.primary ? Theme.accentText : Theme.primaryText
                strokeWidth: 2
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: control.text
                font: control.font
                color: control.primary
                       ? Theme.accentText
                       : (control.enabled ? Theme.primaryText : Theme.secondaryText)
            }
        }
    }

    background: Rectangle {
        id: bg

        radius: 13
        border.width: control.primary ? 0 : 1
        border.color: control.hovered ? Theme.accent : Theme.border
        opacity: control.enabled ? 1.0 : 0.45

        color: control.primary
               ? "transparent"
               : (control.down ? Theme.hover
                                : (control.hovered ? Qt.lighter(Theme.elevated, Theme.dark ? 1.18 : 1.03)
                                                   : Theme.elevated))

        gradient: control.primary ? primaryGradient : null

        Gradient {
            id: primaryGradient

            GradientStop {
                position: 0.0
                color: control.down ? Theme.accentPressedTop
                                     : (control.hovered ? Qt.lighter(Theme.accentTop, 1.06) : Theme.accentTop)
            }
            GradientStop {
                position: 1.0
                color: control.down ? Theme.accentPressedBottom
                                     : (control.hovered ? Qt.lighter(Theme.accentBottom, 1.06) : Theme.accentBottom)
            }
        }

        // Subtle top highlight for depth on the primary variant.
        Rectangle {
            visible: control.primary
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 2
            anchors.rightMargin: 2
            anchors.topMargin: 1
            height: parent.height / 2
            radius: parent.radius - 2
            color: "#FFFFFF"
            opacity: 0.12
        }

        Behavior on color {
            ColorAnimation {
                duration: control.animationDuration
            }
        }

        Behavior on border.color {
            ColorAnimation {
                duration: control.animationDuration
            }
        }
    }
}
