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

    // Accent / primary (deep modern blue)
    readonly property color accent:       "#2F6BED"
    readonly property color accentTop:    "#3B79F2"
    readonly property color accentBottom: "#1E54D6"
    readonly property color accentPressedTop:    "#2E64DE"
    readonly property color accentPressedBottom: "#1645BC"
    readonly property color accentText:   "#FFFFFF"

    // Text
    readonly property color primaryText:   dark ? "#F2F5F8" : "#1A1F25"
    readonly property color secondaryText: dark ? "#AEB8C2" : "#5B6672"

    // States
    readonly property color hover:           dark ? "#26323C" : "#E2E8F0"
    readonly property color selectionBg:     dark ? "#2D79B8" : "#4D8DFF"
    readonly property color selectionBorder: dark ? "#67C1FF" : "#9CC1FF"
    readonly property color selectionText:   "#FFFFFF"
    readonly property color selectionSubText: dark ? "#D7EEFF" : "#EAF2FF"

    // Pills / badges
    readonly property color pillFill:   dark ? "#24313B" : "#E4ECFB"
    readonly property color pillBorder: dark ? "#344B5D" : "#C3D6F5"
    readonly property color pillText:   dark ? "#BFE3FF" : "#2E6FE0"

    readonly property color success: "#4CAF50"
}
