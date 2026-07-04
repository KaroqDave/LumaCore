// SPDX-License-Identifier: GPL-2.0-or-later

pragma Singleton
import QtQuick

QtObject {
    id: theme

    // "Auto" | "Light" | "Dark"
    property string mode: "Dark"

    readonly property bool systemDark: Application.styleHints.colorScheme !== Qt.Light
    readonly property bool dark: mode === "Light"
                                ? false
                                : (mode === "Dark" ? true : systemDark)

    readonly property int animationDuration: 160

    // Corner radii: large for outer cards/panels, small for inner controls.
    readonly property int radiusSmall: 8
    readonly property int radiusMedium: 10
    readonly property int radiusLarge: 14

    // Surfaces. Borders and dividers are translucent hairlines so they stay
    // subtle over every surface tone instead of reading as heavy outlines.
    readonly property color window:   dark ? "#0C1116" : "#F2F5F9"
    readonly property color surface:  dark ? "#151C24" : "#FFFFFF"
    readonly property color elevated: dark ? "#1D2630" : "#F7FAFD"
    readonly property color inputBg:  dark ? "#111820" : "#EEF3F8"
    readonly property color border:   dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0.10, 0.17, 0.26, 0.14)
    readonly property color divider:  dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0.10, 0.17, 0.26, 0.10)
    readonly property color treeLine: dark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0.18, 0.30, 0.44, 0.32)
    readonly property color subtleSurface: dark ? "#12181F" : "#FBFCFE"
    readonly property color sunken: dark ? "#0C1218" : "#E9EFF6"

    // Accent / primary (clear modern blue, flat fills)
    readonly property color accent:        "#2F6FED"
    readonly property color accentHover:   "#3F7DF2"
    readonly property color accentPressed: "#2457C8"
    readonly property color accentSoft: dark ? "#1B3158" : "#E7F0FF"
    readonly property color accentSoftHover: dark ? "#203A68" : "#DCE9FF"
    readonly property color accentSoftBorder: dark ? "#315DA4" : "#BBD2FF"
    readonly property color accentText:   "#FFFFFF"
    readonly property color defaultColor: accent

    // Text
    readonly property color primaryText:   dark ? "#F4F7FA" : "#18212B"
    readonly property color secondaryText: dark ? "#AAB6C2" : "#596878"
    readonly property color mutedText: dark ? "#7F8B98" : "#7B8795"

    // States
    readonly property color hover:           dark ? "#1F2933" : "#E8EEF6"
    readonly property color hoverStrong:     dark ? "#263240" : "#DDE7F3"
    readonly property color selectionBg:     dark ? "#244F9D" : "#2F6FED"
    readonly property color selectionBorder: dark ? "#4D86F7" : "#5D91F5"
    readonly property color selectionText:   "#FFFFFF"
    readonly property color selectionSubText: dark ? "#D6E5FF" : "#EAF2FF"

    // Pills / badges
    readonly property color pillFill:   accentSoft
    readonly property color pillBorder: accentSoftBorder
    readonly property color pillText:   dark ? "#A8C7FF" : "#225ACD"

    readonly property color success: "#42B883"
    readonly property color warning: "#E9A23B"
    readonly property color warningBg: dark ? "#332817" : "#FFF5DD"
    readonly property color error: "#EF5A5A"
    readonly property color errorBg: dark ? "#351B1E" : "#FEE7E7"

    // Format a QML color as an uppercase #RRGGBB string. Shared so the various
    // panels and dialogs do not each carry their own copy of this conversion.
    function colorToHex(value) {
        const red = Math.round(value.r * 255).toString(16).padStart(2, "0")
        const green = Math.round(value.g * 255).toString(16).padStart(2, "0")
        const blue = Math.round(value.b * 255).toString(16).padStart(2, "0")
        return ("#" + red + green + blue).toUpperCase()
    }
}
