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

    // Surfaces
    readonly property color window:   dark ? "#15191D" : "#EDF1F6"
    readonly property color surface:  dark ? "#1E242A" : "#FFFFFF"
    readonly property color elevated: dark ? "#252B32" : "#F3F6FA"
    readonly property color inputBg:  dark ? "#171C21" : "#E9EEF4"
    readonly property color border:   dark ? "#343C44" : "#D5DBE3"
    readonly property color divider:  dark ? "#343C44" : "#DCE2EA"
    readonly property color treeLine:   dark ? "#3A424C" : "#C8D0DA"

    // Accent / primary (deep modern blue)
    readonly property color accent:       "#1E54D6"
    readonly property color accentTop:    "#2F6BED"
    readonly property color accentBottom: "#163DA8"
    readonly property color accentPressedTop:    "#1A4AC4"
    readonly property color accentPressedBottom: "#123590"
    readonly property color accentText:   "#FFFFFF"
    readonly property color defaultColor: accent

    // Text
    readonly property color primaryText:   dark ? "#F2F5F8" : "#1A1F25"
    readonly property color secondaryText: dark ? "#AEB8C2" : "#5B6672"

    // States
    readonly property color hover:           dark ? "#26323C" : "#E2E8F0"
    readonly property color selectionBg:     dark ? "#1A4490" : "#2F6BED"
    readonly property color selectionBorder: dark ? "#2F6BED" : "#5A9AFF"
    readonly property color selectionText:   "#FFFFFF"
    readonly property color selectionSubText: dark ? "#C8DCFF" : "#EAF2FF"

    // Pills / badges
    readonly property color pillFill:   dark ? "#1A2A3D" : "#E4ECFB"
    readonly property color pillBorder: dark ? "#2A4470" : "#A8C4F0"
    readonly property color pillText:   dark ? "#8BB8FF" : "#1E54D6"

    readonly property color success: "#4CAF50"
    readonly property color warning: "#F59E0B"
}
