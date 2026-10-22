import QtQuick
import QtTest
import Codicier

TestCase {
    name: "Theme"

    readonly property var textRoles: ["ink", "inkMuted", "mark", "accent"]
    readonly property var backgrounds: ["paper", "surface"]

    // Relative luminance and contrast ratio from WCAG 2.
    function luminance(c: color): real {
        const lin = v => v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4);
        return 0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b);
    }

    function contrast(a: string, b: string): real {
        const la = luminance(Qt.color(a));
        const lb = luminance(Qt.color(b));
        return (Math.max(la, lb) + 0.05) / (Math.min(la, lb) + 0.05);
    }

    function cleanup() {
        Settings.theme = "system";
        Settings.accent = "theme";
        Theme.motion.reduced = false;
    }

    function test_themes() {
        compare(Object.keys(Theme.palettes).sort(), ["contrast", "dark", "light", "mist", "night", "sepia"]);
    }

    // Each accent passes 4.5 on the paper and surface of each theme. The
    // welcome title uses ink, so test_textContrast covers it.
    function test_accentContrast() {
        for (const accent of Object.keys(Theme.accents)) {
            Settings.accent = accent;
            for (const theme of Object.keys(Theme.palettes)) {
                Settings.theme = theme;
                for (const bg of backgrounds) {
                    const ratio = contrast(Theme.color.accent, Theme.color[bg]);
                    verify(ratio >= 4.5, `${accent} on ${theme} ${bg}: ${ratio.toFixed(2)}`);
                }
            }
        }
    }

    function test_accentReplacesTheAccentOfTheTheme() {
        Settings.theme = "light";
        Settings.accent = "green";
        compare(Theme.color.accent, Theme.accents.green[0]);
        compare(Theme.color.focus, Theme.accents.green[0]);
        Settings.theme = "night";
        compare(Theme.color.accent, Theme.accents.green[1]);
        Settings.accent = "theme";
        compare(Theme.color.accent, Theme.palettes.night.accent);
    }

    // High contrast: WCAG AAA, 7 or more, for every text role, also with an
    // accent picked.
    function test_highContrastIsAAA() {
        Settings.theme = "contrast";
        Settings.accent = "plum";
        for (const text of textRoles)
            for (const bg of backgrounds.concat(["selection"]))
                verify(contrast(Theme.color[text], Theme.color[bg]) >= 7, `${text} on ${bg}`);
    }

    function test_textContrast_data() {
        const rows = [];
        for (const theme of Object.keys(Theme.palettes))
            for (const text of textRoles)
                for (const bg of backgrounds)
                    rows.push({ tag: `${theme} ${text} on ${bg}`, theme, text, bg });
        for (const theme of Object.keys(Theme.palettes))
            rows.push({ tag: `${theme} ink on selection`, theme, text: "ink", bg: "selection" });
        return rows;
    }

    function test_textContrast(row) {
        const palette = Theme.palettes[row.theme];
        verify(palette[row.text] !== undefined, `missing ${row.text}`);
        verify(palette[row.bg] !== undefined, `missing ${row.bg}`);
        const ratio = contrast(palette[row.text], palette[row.bg]);
        verify(ratio >= 4.5, `${row.tag}: ${ratio.toFixed(2)}`);
    }

    function test_focusIsVisible() {
        for (const theme of Object.keys(Theme.palettes)) {
            const palette = Theme.palettes[theme];
            for (const bg of backgrounds)
                verify(contrast(palette.focus, palette[bg]) >= 3, `${theme} focus on ${bg}`);
        }
    }

    function test_marksAreQuieterThanInk() {
        for (const theme of Object.keys(Theme.palettes)) {
            const palette = Theme.palettes[theme];
            verify(contrast(palette.mark, palette.paper) < contrast(palette.ink, palette.paper),
                   theme);
        }
    }

    function test_choiceSelectsPalette() {
        Settings.theme = "sepia";
        compare(Theme.name, "sepia");
        compare(Theme.color.paper, Theme.palettes.sepia.paper);
    }

    function test_systemFollowsColorScheme() {
        Settings.theme = "system";
        const dark = Application.styleHints.colorScheme === Qt.ColorScheme.Dark;
        compare(Theme.name, dark ? "dark" : "light");
    }

    function test_bundledFontsLoad() {
        for (const loader of [Theme.readingFont, Theme.readingItalicFont, Theme.uiFont])
            compare(loader.status, FontLoader.Ready, loader.source);
        compare(Theme.type.reading, "Literata");
        compare(Theme.type.ui, "Inter");
    }

    function test_readingLineIsInPixels() {
        compare(Theme.type.readingLine, Math.round(Theme.type.readingSize * Theme.type.readingLineHeight));
    }

    function test_reducedMotionStopsMotion() {
        Theme.motion.reduced = true;
        compare(Theme.motion.fast, 0);
        compare(Theme.motion.slow, 0);
        Theme.motion.reduced = false;
        verify(Theme.motion.normal > 0);
    }
}
