pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Templates as T
import QtTest
import Codicier

// Every control has a name for a screen reader, and Tab reaches every
// control outside a list. A list is reached as one control, and its arrow
// keys move between its rows. The page is the one exception: Tab is a key of
// the text, so F6 reaches the page.
TestCase {
    id: testCase

    name: "Accessibility"
    when: windowShown
    visible: true
    width: 1400
    height: 900

    ProjectList {
        id: workspace
    }

    Component {
        id: viewComponent

        ProjectView {
            anchors.fill: parent
        }
    }

    function makeView(): ProjectView {
        const temp = StandardPaths.writableLocation(StandardPaths.TempLocation).toString().replace("file://", "");
        workspace.folder = temp + "/codicier-a11y-" + Date.now() + "-" + Math.floor(Math.random() * 1e6);
        compare(workspace.create("Harbour", "novel", ""), "");
        const view = createTemporaryObject(viewComponent, testCase, { folder: workspace.folder + "/harbour" }) as ProjectView;
        compare(view.session.error, "");
        return view;
    }

    function inList(item: Item): bool {
        for (let parent = item.parent; parent; parent = parent.parent) {
            if (parent instanceof ListView)
                return true;
        }
        return false;
    }

    // The controls under root with no name, or that Tab cannot reach.
    function faults(root: Item): list<string> {
        let found = [];
        const walk = item => {
            if (!item || !item.visible)
                return;
            const control = item instanceof T.AbstractButton || item instanceof TextInput
                || item instanceof TextEdit;
            if (control && item.objectName !== "editor" && !(item instanceof PageText)) {
                const label = String(item).split("(")[0] + " " + (item.objectName || item.text || "");
                if (!item.Accessible.name)
                    found.push("no name: " + label);
                const tab = item instanceof T.Control ? (item.focusPolicy & Qt.TabFocus) !== 0
                                                      : item.activeFocusOnTab;
                if (!tab && !inList(item))
                    found.push("no Tab: " + label);
            }
            for (let i = 0; i < item.children.length; ++i)
                walk(item.children[i]);
        };
        walk(root);
        return found;
    }

    function test_projectViewHasNoFaults() {
        const view = makeView();
        const model = view.session.binder;
        // A scene, so the inspector shows every field.
        let scene = "";
        for (const item of view.session.items()) {
            if (item.kind === "scene" && scene === "")
                scene = item.id;
        }
        view.session.currentId = scene;
        view.inspectorOpen = true;
        waitForRendering(view);
        compare(faults(view), []);

        view.openSearch(true);
        waitForRendering(view);
        compare(faults(view), []);

        view.commands[0].run();
        tryVerify(() => (findChild(view.T.Overlay.overlay, "paletteField") as Item)?.visible ?? false);
        compare(faults(view.T.Overlay.overlay), []);
        keyClick(Qt.Key_Escape);
    }

    Component {
        id: settingsComponent

        SettingsDialog {}
    }

    Component {
        id: newProjectComponent

        NewProjectDialog {
            projects: workspace
        }
    }

    function test_dialogsHaveNoFaults() {
        for (const component of [settingsComponent, newProjectComponent]) {
            const dialog = createTemporaryObject(component, testCase) as T.Popup;
            dialog.open();
            tryVerify(() => dialog.opened);
            compare(faults(dialog.contentItem), []);
            dialog.close();
        }
    }

    function test_f6MovesThroughThePanes() {
        const view = makeView();
        const model = view.session.binder;
        view.session.currentId = model.data(model.index(2, 0), BinderModel.IdRole);
        view.inspectorOpen = true;
        waitForRendering(view);
        const text = findChild(view, "editor") as TextEdit;
        const binderPane = findChild(view, "binderPane") as Item;
        const inPane = pane => {
            for (let item = view.Window.activeFocusItem; item; item = item.parent) {
                if (item === pane)
                    return true;
            }
            return false;
        };

        view.forceActiveFocus();
        keyClick(Qt.Key_F6);
        verify(inPane(binderPane));
        keyClick(Qt.Key_F6);
        verify(text.activeFocus);
        keyClick(Qt.Key_F6);
        verify(!inPane(binderPane) && !text.activeFocus);
        keyClick(Qt.Key_F6);
        verify(inPane(binderPane));

        // A closed pane is not in the order.
        view.inspectorOpen = false;
        keyClick(Qt.Key_F6);
        verify(text.activeFocus);
        keyClick(Qt.Key_F6);
        verify(inPane(binderPane));
    }

    function test_arrowKeysMoveTheFocusThroughTheBinder() {
        const view = makeView();
        view.forceActiveFocus();
        keyClick(Qt.Key_F6);
        const first = view.Window.activeFocusItem;
        verify(first instanceof BinderRow, String(first));
        keyClick(Qt.Key_Down);
        const second = view.Window.activeFocusItem;
        verify(second instanceof BinderRow, String(second));
        verify(second !== first);
        verify((second as BinderRow).Accessible.name !== "");
    }

    Component {
        id: deleteComponent

        DeleteProjectDialog {
            projects: workspace
        }
    }

    TextInput {
        id: source

        text: "Harbour"
    }

    function test_deleteNeedsTheTitleTypedByHand() {
        makeView().destroy();
        const folder = workspace.folder + "/harbour";
        const dialog = createTemporaryObject(deleteComponent, testCase) as DeleteProjectDialog;
        dialog.ask(folder, "Harbour");
        tryVerify(() => dialog.opened);
        compare(faults(dialog.contentItem), []);
        const field = findChild(dialog.contentItem, "confirmTitle") as TextField;
        const button = findChild(dialog.contentItem, "confirmDelete") as Button;
        tryVerify(() => field.activeFocus);

        // Paste does nothing.
        source.selectAll();
        source.copy();
        keyClick(Qt.Key_V, Qt.ControlModifier);
        keyClick(Qt.Key_Insert, Qt.ShiftModifier);
        compare(field.text, "");

        for (const c of "Harbou")
            keyClick(c);
        verify(!button.enabled);
        keyClick(Qt.Key_Return);
        verify(dialog.opened);
        keyClick("r");
        verify(button.enabled);
        keyClick(Qt.Key_Return);
        compare(dialog.error, "");
        tryVerify(() => !dialog.opened);
        verify(!workspace.get(0).folder || workspace.get(0).folder !== folder);
        compare(workspace.count, 0);
    }
}
