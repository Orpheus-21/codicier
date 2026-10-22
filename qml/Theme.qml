pragma Singleton
pragma ComponentBehavior: Bound

import QtQuick

// Every colour, size, space, radius, and duration in the app comes from here.
// docs/design/brief.md gives the reasons for the values.
QtObject {
    id: theme

    // The theme the author picked: "system", "light", "dark", "sepia",
    // "night", "mist", or "contrast".
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
                "focus": "#94552a",
                // The chart series, in fixed order. dataviz validate_palette passes
                // them on this paper; the light ones need labels or a table.
                "series": ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4"]
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
                "focus": "#d4935f",
                // The chart series, in fixed order. dataviz validate_palette passes
                // them on this paper; the light ones need labels or a table.
                "series": ["#3987e5", "#d95926", "#199e70", "#c98500", "#d55181"]
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
                "focus": "#83491f",
                // The chart series, in fixed order. dataviz validate_palette passes
                // them on this paper; the light ones need labels or a table.
                "series": ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4"]
            },
            "night": {
                "paper": "#161a20",
                "surface": "#1d222a",
                "selection": "#2e3846",
                "rule": "#2c333d",
                "ink": "#e3e7ee",
                "inkMuted": "#a3acb9",
                "mark": "#868e99",
                "accent": "#7fb0e0",
                "focus": "#7fb0e0",
                "series": ["#3987e5", "#d95926", "#199e70", "#c98500", "#d55181"]
            },
            "mist": {
                "paper": "#f4f6f8",
                "surface": "#e9edf1",
                "selection": "#d3dce6",
                "rule": "#d8dee5",
                "ink": "#1f2730",
                "inkMuted": "#4e5966",
                "mark": "#5e6874",
                "accent": "#2f5f8f",
                "focus": "#2f5f8f",
                "series": ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4"]
            },
            // High contrast: every text role has a contrast of 7 or more.
            "contrast": {
                "paper": "#ffffff",
                "surface": "#f0f0f0",
                "selection": "#c4d8f5",
                "rule": "#767676",
                "ink": "#000000",
                "inkMuted": "#262626",
                "mark": "#383838",
                "accent": "#6a2600",
                "focus": "#6a2600",
                "series": ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4"]
            }
        })
    readonly property var darkThemes: ["dark", "night"]
    // The accent colours the author can pick, as [on a light paper, on a dark
    // paper]. Each passes 4.5 on the paper and surface of each theme.
    readonly property var accents: ({
            "blue": ["#2d5f96", "#7fb0e0"],
            "green": ["#2f6b45", "#79bf8e"],
            "plum": ["#7a3d6e", "#d39ac6"],
            "red": ["#a33a2e", "#ec8f7f"]
        })
    // The palette with the accent of the author. The high-contrast theme keeps
    // its own accent, so its text stays at 7 or more.
    readonly property var color: {
        const pick = accents[Settings.accent];
        if (pick === undefined || name === "contrast")
            return palettes[name];
        const tone = pick[darkThemes.includes(name) ? 1 : 0];
        return Object.assign({}, palettes[name], { "accent": tone, "focus": tone });
    }
    // The colour labels of binder items, from the chart series of the theme.
    readonly property var labels: ["blue", "orange", "green", "yellow", "pink"]

    function labelColor(label: string): color {
        const at = labels.indexOf(label);
        return at < 0 ? "transparent" : color.series[at];
    }

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
        readonly property real readingLineHeight: Settings.lineSpacing / 100
        // The same line height in pixels. Text.lineHeight multiplies the line
        // spacing of the font, which is wider than the font size, so a Text
        // item must use Text.FixedHeight with this value.
        readonly property int readingLine: Math.round(readingSize * readingLineHeight)
        // Characters per line in the writing view.
        readonly property int measure: Settings.measure
        // The sizes of the menus and panels follow the setting of the author.
        readonly property real uiScale: Settings.uiScale / 100
        readonly property int small: Math.round(12 * uiScale)
        readonly property int body: Math.round(14 * uiScale)
        readonly property int large: Math.round(17 * uiScale)
        readonly property int title: Math.round(24 * uiScale)
        readonly property int display: Math.round(32 * uiScale)
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
