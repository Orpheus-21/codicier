import QtQuick
import QtQuick.Templates as T
import QtTest
import Codicier

TestCase {
    id: testCase

    name: "Components"
    when: windowShown
    // TestCase hides its children by default, and a hidden item draws nothing.
    visible: true
    width: 400
    height: 300

    Component {
        id: buttonComponent

        Button {
            text: "Continue writing"
        }
    }

    Component {
        id: iconButtonComponent

        IconButton {
            iconName: "search"
            label: "Search"
        }
    }

    Component {
        id: textFieldComponent

        TextField {
            placeholderText: "Filter the binder"
        }
    }

    Component {
        id: listItemComponent

        ListItem {
            text: "A Date She Did Not Know"
            trailing: "1,284"
        }
    }

    Component {
        id: paneComponent

        Pane {
            width: 100
            height: 100
        }
    }

    Component {
        id: sizedPaneComponent

        Pane {
            Item {
                implicitWidth: 50
                implicitHeight: 40
            }
        }
    }

    Component {
        id: splitterComponent

        Splitter {
            width: 300
            height: 100

            Rectangle {
                T.SplitView.preferredWidth: 100
                color: "red"
            }

            Rectangle {
                color: "blue"
            }
        }
    }

    Component {
        id: menuComponent

        Menu {
            T.Action {
                text: "Rename"
            }

            T.Action {
                text: "Move to trash"
            }
        }
    }

    SignalSpy {
        id: spy
    }

    function make(component: Component, properties: var): Item {
        const item = createTemporaryObject(component, testCase, properties ?? {});
        verify(item, "the component did not load");
        return item;
    }

    function pixelAt(item: Item, x: int, y: int): color {
        waitForRendering(item);
        return grabImage(item).pixel(x, y);
    }

    function cleanup() {
        Settings.theme = "system";
    }

    function test_everyComponentHasSize_data() {
        return [
            { tag: "Button", component: buttonComponent },
            { tag: "IconButton", component: iconButtonComponent },
            { tag: "TextField", component: textFieldComponent },
            { tag: "ListItem", component: listItemComponent },
            { tag: "Pane", component: paneComponent },
            { tag: "Splitter", component: splitterComponent }
        ];
    }

    function test_everyComponentHasSize(row) {
        const item = make(row.component);
        verify(item.width > 0 && item.height > 0, `${item.width} x ${item.height}`);
    }

    function test_primaryButtonFillsWithAccent() {
        Settings.theme = "light";
        const button = make(buttonComponent, { kind: "primary" }) as Button;
        verify(Qt.colorEqual(pixelAt(button, 4, button.height / 2), Theme.color.accent));
        compare(button.foreground, Qt.color(Theme.color.paper));
    }

    function test_quietButtonHasNoFill() {
        Settings.theme = "light";
        const button = make(buttonComponent) as Button;
        verify(!Qt.colorEqual(pixelAt(button, 4, button.height / 2), Theme.color.accent));
        compare(button.foreground, Qt.color(Theme.color.ink));
    }

    function test_buttonClicks() {
        const button = make(buttonComponent) as Button;
        spy.target = button;
        spy.signalName = "clicked";
        spy.clear();
        mouseClick(button);
        compare(spy.count, 1);
    }

    function test_iconButtonDrawsAndHasName() {
        const button = make(iconButtonComponent);
        compare(button.Accessible.name, "Search");
        waitForRendering(button);
        const image = grabImage(button);
        const background = image.pixel(0, 0);
        let inked = 0;
        for (let x = 0; x < image.width; ++x)
            for (let y = 0; y < image.height; ++y)
                if (!Qt.colorEqual(image.pixel(x, y), background))
                    ++inked;
        verify(inked > 10, "the icon drew nothing");
    }

    function test_textFieldShowsPlaceholderWhenEmpty() {
        const field = make(textFieldComponent);
        waitForRendering(field);
        const image = grabImage(field);
        const background = image.pixel(field.width - 4, field.height / 2);
        let inked = 0;
        for (let x = field.leftPadding; x < field.leftPadding + 60; ++x)
            for (let y = 4; y < field.height - 4; ++y)
                if (!Qt.colorEqual(image.pixel(x, y), background))
                    ++inked;
        verify(inked > 20, "no placeholder text");
    }

    function test_paneSizesToContent() {
        const pane = createTemporaryObject(sizedPaneComponent, testCase) as Pane;
        verify(pane);
        compare(pane.implicitWidth, 50 + 2 * Theme.space.l);
        compare(pane.implicitHeight, 40 + 2 * Theme.space.l);
    }

    function test_textFieldTakesText() {
        const field = make(textFieldComponent);
        field.forceActiveFocus();
        keyClick(Qt.Key_A);
        keyClick(Qt.Key_B);
        compare(field.text, "ab");
    }

    function test_textFieldFocusUsesFocusColour() {
        Settings.theme = "light";
        const field = make(textFieldComponent);
        field.forceActiveFocus();
        verify(Qt.colorEqual(pixelAt(field, field.width / 2, 0), Theme.color.focus));
    }

    function test_highlightedListItemUsesSelection() {
        Settings.theme = "light";
        const item = make(listItemComponent, { highlighted: true });
        verify(Qt.colorEqual(pixelAt(item, item.width / 2, 2), Theme.color.selection));
    }

    function test_paneFollowsTheme() {
        const pane = make(paneComponent);
        Settings.theme = "light";
        verify(Qt.colorEqual(pixelAt(pane, 50, 50), Theme.palettes.light.surface));
        Settings.theme = "dark";
        verify(Qt.colorEqual(pixelAt(pane, 50, 50), Theme.palettes.dark.surface));
    }

    function test_splitterDrawsARule() {
        Settings.theme = "light";
        const splitter = make(splitterComponent);
        compare(splitter.count, 2);
        const first = splitter.itemAt(0);
        // The rule is the first pixel of the handle, right after the first pane.
        verify(Qt.colorEqual(pixelAt(splitter, first.width, 50), Theme.color.rule));
    }

    function test_splitterHandleGrabsBesideTheRule() {
        const splitter = make(splitterComponent);
        const first = splitter.itemAt(0);
        const before = first.width;
        waitForRendering(splitter);
        // Press 3 pixels right of the rule, then drag 40 pixels.
        mousePress(splitter, before + 3, 50);
        mouseMove(splitter, before + 23, 50);
        mouseMove(splitter, before + 43, 50);
        mouseRelease(splitter, before + 43, 50);
        verify(first.width > before + 30, `${before} -> ${first.width}`);
    }

    function test_splitterRuleFacesTheOuterPane() {
        Settings.theme = "light";
        const splitter = make(splitterComponent);
        splitter.itemAt(0).T.SplitView.preferredWidth = 220;
        waitForRendering(splitter);
        const first = splitter.itemAt(0);
        verify(first.width > 150);
        // A handle in the right half has its rule at its end.
        verify(Qt.colorEqual(pixelAt(splitter, first.width + 6, 50), Theme.color.rule));
        verify(!Qt.colorEqual(pixelAt(splitter, first.width, 50), Theme.color.rule));
    }

    function test_menuOpensAndTriggers() {
        const menu = createTemporaryObject(menuComponent, testCase) as Menu;
        verify(menu);
        compare(menu.count, 2);
        spy.target = menu.actionAt(0);
        spy.signalName = "triggered";
        spy.clear();
        menu.open();
        tryCompare(menu, "opened", true);
        const first = menu.itemAt(0);
        verify(first.width > 0);
        mouseClick(first);
        compare(spy.count, 1);
        tryCompare(menu, "visible", false);
    }
}
