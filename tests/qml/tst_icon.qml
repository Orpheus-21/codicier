import QtQuick
import QtTest
import Codicier

import "../../qml/IconPaths.js" as IconPaths

TestCase {
    id: testCase

    name: "Icon"
    when: windowShown
    // TestCase hides its children by default, and a hidden item draws nothing.
    visible: true
    width: 64
    height: 64

    Component {
        id: iconComponent

        Icon {
            size: 48
        }
    }

    function makeIcon(iconName: string): Icon {
        return createTemporaryObject(iconComponent, testCase, { name: iconName }) as Icon;
    }

    // Count the pixels that differ from the background. The icons keep a
    // margin of 2 units, so the corner pixel is always background.
    function inkedPixels(item: Item): int {
        waitForRendering(item);
        const image = grabImage(item);
        const background = image.pixel(0, 0);
        let count = 0;
        for (let x = 0; x < image.width; x += 2)
            for (let y = 0; y < image.height; y += 2)
                if (!Qt.colorEqual(image.pixel(x, y), background))
                    ++count;
        return count;
    }

    function test_everyIconDraws_data() {
        return Object.keys(IconPaths.paths).map(n => ({ tag: n, iconName: n }));
    }

    function test_everyIconDraws(row) {
        const icon = makeIcon(row.iconName);
        verify(icon.path.length > 0);
        verify(inkedPixels(icon) > 10, row.iconName);
    }

    function test_unknownNameDrawsNothing() {
        const icon = makeIcon("no-such-icon");
        compare(icon.path, "");
        compare(inkedPixels(icon), 0);
    }

    function test_colourFollowsTheme() {
        const icon = makeIcon("search");
        Settings.theme = "dark";
        compare(icon.color, Qt.color(Theme.palettes.dark.ink));
        Settings.theme = "light";
        compare(icon.color, Qt.color(Theme.palettes.light.ink));
        Settings.theme = "system";
    }

    function test_sizeSetsBounds() {
        const icon = makeIcon("plus");
        compare(icon.width, 48);
        compare(icon.height, 48);
    }
}
