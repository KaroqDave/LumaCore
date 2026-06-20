import QtQuick
import QtQuick.Controls
import LumaCore

Item {
    id: panel

    property var controller
    property bool stickToBottom: true

    readonly property bool contentOverflows: verticalBar.size < 0.999

    implicitWidth: 200
    implicitHeight: 200

    ScrollView {
        id: scrollView

        anchors.fill: parent
        clip: true

        onAvailableWidthChanged: Qt.callLater(panel.evaluateScrollState)
        onHeightChanged: Qt.callLater(panel.evaluateScrollState)

        ScrollBar.vertical: ScrollBar {
            id: verticalBar

            policy: ScrollBar.AsNeeded

            onPositionChanged: Qt.callLater(panel.evaluateScrollState)
            onSizeChanged: Qt.callLater(panel.evaluateScrollState)
        }

        TextArea {
            id: logArea

            width: scrollView.availableWidth
            text: panel.controller ? panel.controller.logText : ""
            readOnly: true
            wrapMode: TextArea.Wrap
            color: Theme.primaryText
            selectedTextColor: Theme.primaryText
            selectionColor: Theme.accent
            font.family: "monospace"
            font.pixelSize: 12

            background: Rectangle {
                color: Theme.inputBg
                radius: 12
                border.color: Theme.border
            }

            onTextChanged: Qt.callLater(panel.evaluateScrollState)
            onContentHeightChanged: Qt.callLater(panel.evaluateScrollState)
        }
    }

    AppButton {
        id: jumpButton

        visible: panel.contentOverflows && !panel.stickToBottom
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10
        width: 120
        height: 28
        variant: "secondary"
        text: qsTr("Jump to latest")
        onClicked: panel.scrollToBottom()
    }

    function scrollToBottom() {
        panel.stickToBottom = true
        logArea.cursorPosition = logArea.length
        verticalBar.position = 1.0 - verticalBar.size
    }

    function evaluateScrollState() {
        if (!panel.controller || panel.controller.logText.length === 0) {
            panel.stickToBottom = true
            return
        }

        if (!panel.contentOverflows) {
            panel.stickToBottom = true
            logArea.cursorPosition = logArea.length
            return
        }

        const atBottom = verticalBar.position >= (1.0 - verticalBar.size - 0.02)
        panel.stickToBottom = atBottom

        if (panel.stickToBottom) {
            logArea.cursorPosition = logArea.length
        }
    }

    Connections {
        target: panel.controller

        function onLogTextChanged() {
            Qt.callLater(panel.evaluateScrollState)
            if (panel.stickToBottom) {
                Qt.callLater(panel.scrollToBottom)
            }
        }
    }
}
