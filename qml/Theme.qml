pragma Singleton
pragma ComponentBehavior: Bound

import QtQuick

// Every colour, size, space, radius, and duration in the app comes from here.
// docs/design/brief.md gives the reasons for the values.
QtObject {
    id: theme

    // The theme the author picked: "system", "light", "dark", or "sepia".
    property string choice: Settings.theme
    // "system" follows the light or dark setting of the desktop.
    readonly property string name: {
        if (choice !== "system")
            return choice;
        return Application.styleHints.colorScheme === Qt.ColorScheme.Dark ? "dark" : "light";
    }

    // Text roles: ink, inkMuted, mark, accent. Each one has a contrast of
    // 4.5 or more on paper and surface. tst_theme.qml checks it.
    readonly property var palettes: ({
            "light": {
                "paper": "#fbf8f1",
                "surface": "#f3eee3",
                "selection": "#e6d9bf",
                "rule": "#e2dacb",
                "ink": "#2b2620",
                "inkMuted": "#62584a",
                "mark": "#706b65",
                "accent": "#94552a",
                "focus": "#94552a"
            },
            "dark": {
                "paper": "#1c1a17",
                "surface": "#24211d",
                "selection": "#3d362c",
                "rule": "#37322b",
                "ink": "#e8e1d5",
                "inkMuted": "#aea596",
                "mark": "#8f8a83",
                "accent": "#d4935f",
                "focus": "#d4935f"
            },
            "sepia": {
                "paper": "#f2e8d2",
                "surface": "#e9ddc3",
                "selection": "#dbc9a5",
                "rule": "#d8c9aa",
                "ink": "#3a2e21",
                "inkMuted": "#5e4d39",
                "mark": "#6c6051",
                "accent": "#83491f",
                "focus": "#83491f"
            }
        })
    readonly property var color: palettes[name]

    // The app bundles its fonts, so the text looks the same on each system.
    // The licences are in fonts/.
    readonly property FontLoader readingFont: FontLoader {
        source: "../fonts/Literata.ttf"
    }
    readonly property FontLoader readingItalicFont: FontLoader {
        source: "../fonts/Literata-Italic.ttf"
    }
    readonly property FontLoader uiFont: FontLoader {
        source: "../fonts/Inter.ttf"
    }

    component TypeScale: QtObject {
        // The fonts of the settings, or the fonts that come with the app.
        readonly property string reading: Settings.readingFont || theme.readingFont.font.family
        readonly property string ui: Settings.uiFont || theme.uiFont.font.family
        readonly property int readingSize: Settings.readingSize
        readonly property real readingLineHeight: 1.5
        // The same line height in pixels. Text.lineHeight multiplies the line
        // spacing of the font, which is wider than the font size, so a Text
        // item must use Text.FixedHeight with this value.
        readonly property int readingLine: Math.round(readingSize * readingLineHeight)
        // Characters per line in the writing view.
        readonly property int measure: Settings.measure
        readonly property int small: 12
        readonly property int body: 14
        readonly property int large: 17
        readonly property int title: 24
        readonly property int display: 32
    }
    readonly property TypeScale type: TypeScale {}

    // A scale of 4 px steps.
    component Spacing: QtObject {
        readonly property int xs: 4
        readonly property int s: 8
        readonly property int m: 12
        readonly property int l: 16
        readonly property int xl: 24
        readonly property int xxl: 32
        readonly property int xxxl: 48
    }
    readonly property Spacing space: Spacing {}

    component Radii: QtObject {
        readonly property int small: 3
        readonly property int medium: 6
    }
    readonly property Radii radius: Radii {}

    component Motion: QtObject {
        // Qt gives no system hint for less motion, so a setting sets this.
        property bool reduced: Settings.reducedMotion
        readonly property int fast: reduced ? 0 : 150
        readonly property int normal: reduced ? 0 : 200
        readonly property int slow: reduced ? 0 : 250
        readonly property int easing: Easing.OutCubic
    }
    readonly property Motion motion: Motion {}
}
