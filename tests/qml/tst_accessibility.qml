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

    function test_codexListsEntitiesByType() {
        const view = makeView();
        const place = view.session.add("place");
        view.session.rename(place, "The Harbour");
        const first = view.session.add("character");
        view.session.rename(first, "Mara");
        const second = view.session.add("character");
        view.session.rename(second, "Abel");
        view.session.currentId = "";

        view.forceActiveFocus();
        keyClick(Qt.Key_E, Qt.ControlModifier | Qt.ShiftModifier);
        verify(view.codexOpen);
        const list = findChild(view, "codexList") as ListView;
        // Characters come first, then places; each type in title order.
        compare(list.model.map(item => item.title), ["Abel", "Mara", "The Harbour"]);
        waitForRendering(view);
        compare(faults(view), []);

        // The filter, then Enter opens the entity.
        for (const c of "mar")
            keyClick(c);
        compare(list.count, 1);
        keyClick(Qt.Key_Return);
        compare(view.session.currentId, first);

        // The inspector saves the aliases of an entity.
        view.inspectorOpen = true;
        waitForRendering(view);
        const aliases = findChild(view, "aliasesField") as TextField;
        verify(aliases.visible);
        aliases.forceActiveFocus();
        for (const c of "the clerk, M")
            keyClick(c);
        keyClick(Qt.Key_Tab);
        compare(view.session.fields(first).aliases, ["the clerk", "M"]);
    }

    function test_customTypeWithFields() {
        const view = makeView();
        view.openCodex();
        waitForRendering(view);
        (findChild(view, "codexPanel") as CodexPanel).editTypes();
        const overlay = view.T.Overlay.overlay;
        tryVerify(() => (findChild(overlay, "typeName") as Item)?.visible ?? false);
        compare(faults((findChild(overlay, "typeName") as Item).parent.parent), []);
        mouseClick(findChild(overlay, "newType") as Item);
        const name = findChild(overlay, "typeName") as TextField;
        name.forceActiveFocus();
        for (const c of "Ships")
            keyClick(c);
        const fields = findChild(overlay, "typeFields") as TextField;
        fields.forceActiveFocus();
        for (const c of "Captain, Port")
            keyClick(c);
        keyClick(Qt.Key_Return);
        const ships = view.session.entityTypes().find(type => type.kind === "ships");
        verify(ships !== undefined);
        compare(ships.fields, ["Captain", "Port"]);
        keyClick(Qt.Key_Escape);

        // An entity of the type shows its fields in the inspector.
        const id = view.session.add("ships");
        view.session.currentId = id;
        view.inspectorOpen = true;
        waitForRendering(view);
        const inspector = findChild(view, "inspector") as Item;
        let captain = null;
        const find = item => {
            for (let i = 0; i < item.children.length; ++i) {
                const child = item.children[i];
                if (child instanceof TextField && child.Accessible.name === "Captain" && child.visible)
                    captain = child;
                find(child);
            }
        };
        find(inspector);
        verify(captain !== null);
        captain.forceActiveFocus();
        for (const c of "Ahab")
            keyClick(c);
        keyClick(Qt.Key_Tab);
        compare(view.session.fields(id).values.Captain, "Ahab");
    }

    function test_linksCompleteAndOpen() {
        const view = makeView();
        const mara = view.session.add("character");
        view.session.rename(mara, "Mara Vell");
        view.session.rename(view.session.add("character"), "Abel Crane");
        let scene = "";
        for (const item of view.session.items()) {
            if (item.kind === "scene" && scene === "")
                scene = item.id;
        }
        view.session.currentId = scene;
        const text = findChild(view, "editor") as TextEdit;
        text.text = "";
        text.forceActiveFocus();
        for (const c of "Ask [[ma")
            keyClick(c);
        const list = findChild(view, "linkList") as Item;
        verify(list.visible);
        keyClick(Qt.Key_Return);
        compare(text.text, "Ask [[Mara Vell]]");
        verify(!list.visible);
        compare(text.cursorPosition, text.text.length);

        // Escape closes the list, and typing goes on as text.
        for (const c of " [[")
            keyClick(c);
        verify(list.visible);
        keyClick(Qt.Key_Escape);
        verify(!list.visible);

        // Ctrl+click on the link opens the entity.
        waitForRendering(view);
        const rect = text.positionToRectangle(7);
        mouseClick(text, rect.x + 2, rect.y + rect.height / 2, Qt.LeftButton, Qt.ControlModifier);
        compare(view.session.currentId, mara);
    }

    function test_inspectorListsMentions() {
        const view = makeView();
        let scene = "";
        for (const item of view.session.items()) {
            if (item.kind === "scene" && scene === "")
                scene = item.id;
        }
        view.session.currentId = scene;
        (findChild(view, "editor") as TextEdit).text = "Mara came in.";
        const mara = view.session.add("character");
        view.session.rename(mara, "Mara");
        view.session.currentId = mara;
        view.inspectorOpen = true;
        waitForRendering(view);
        const row = findChild(view, "mention") as ListItem;
        verify(row && row.visible);
        compare(row.trailing, "1");
        compare(faults(findChild(view, "inspector") as Item), []);
        mouseClick(row);
        compare(view.session.currentId, scene);
    }

    function test_corkboardDragReordersTheBinder() {
        const view = makeView();
        let chapter = "";
        for (const item of view.session.items()) {
            if (item.kind === "chapter" && chapter === "")
                chapter = item.id;
        }
        view.session.currentId = chapter;
        view.session.add("scene");
        view.session.currentId = chapter;
        const before = view.session.childrenOf(chapter).map(card => card.id);
        verify(before.length >= 2);

        view.forceActiveFocus();
        keyClick(Qt.Key_K, Qt.ControlModifier | Qt.ShiftModifier);
        compare(view.viewMode, "board");
        const grid = findChild(view, "corkboard") as GridView;
        tryCompare(grid, "count", before.length);
        waitForRendering(view);
        // Drag the second card onto the first card.
        const second = grid.itemAtIndex(1);
        const first = grid.itemAtIndex(0);
        const from = second.mapToItem(view, second.width / 2, second.height / 2);
        const to = first.mapToItem(view, first.width / 2, first.height / 2);
        mousePress(view, from.x, from.y);
        for (let i = 1; i <= 10; ++i)
            mouseMove(view, from.x + (to.x - from.x) * i / 10, from.y + (to.y - from.y) * i / 10);
        mouseRelease(view, to.x, to.y);
        const after = view.session.childrenOf(chapter).map(card => card.id);
        compare(after[0], before[1]);
        compare(after[1], before[0]);

        // A click opens the item, and the key goes back to the page.
        waitForRendering(view);
        const card = grid.itemAtIndex(0);
        mouseClick(card, card.width / 2, card.height / 2);
        compare(view.session.currentId, after[0]);
        compare(view.viewMode, "page");
        keyClick(Qt.Key_K, Qt.ControlModifier | Qt.ShiftModifier);
        compare(view.viewMode, "board");
    }

    function test_outlinerSortsAndOpens() {
        const view = makeView();
        let part = "";
        for (const item of view.session.items()) {
            if (item.kind === "part" && part === "")
                part = item.id;
        }
        verify(part !== "");
        view.session.currentId = view.session.outlineOf(part)[0].id;
        view.session.add("scene");
        const rows = view.session.outlineOf(part);
        verify(rows.length >= 2);
        view.session.setField(rows[0].id, "pov", "Zed");
        view.session.setField(rows[1].id, "pov", "Abel");
        view.session.currentId = part;
        view.forceActiveFocus();
        keyClick(Qt.Key_O, Qt.ControlModifier | Qt.ShiftModifier);
        compare(view.viewMode, "outline");
        const list = findChild(view, "outlinerRows") as ListView;
        tryCompare(list, "count", rows.length);
        waitForRendering(view);
        compare(faults(findChild(view, "outlinerView") as Item), []);

        const header = findChild(view, "header-pov") as Item;
        mouseClick(header);
        compare(list.model[0].pov, "Abel");
        mouseClick(header);
        compare(list.model[0].pov, "Zed");

        const first = list.itemAtIndex(0);
        mouseClick(first);
        compare(view.session.currentId, rows[0].id);
        compare(view.viewMode, "page");
    }

    function test_sessionTargetShowsInTheStatusBar() {
        const view = makeView();
        view.forceActiveFocus();
        keyClick(Qt.Key_G, Qt.ControlModifier | Qt.ShiftModifier);
        const overlay = view.T.Overlay.overlay;
        const field = findChild(overlay, "sessionTarget") as TextField;
        tryVerify(() => field && field.visible);
        compare(faults(field.parent.parent), []);
        field.forceActiveFocus();
        field.selectAll();
        for (const c of "500")
            keyClick(c);
        keyClick(Qt.Key_Tab);
        compare(Settings.sessionTarget, 500);
        keyClick(Qt.Key_Escape);
        const count = findChild(view, "sessionCount") as Item;
        verify(count.visible);
        verify((count.children[0] as Text).text.includes("of 500"));
        Settings.sessionTarget = 0;
    }

    function test_goalAndDeadlineInTheDialog() {
        const view = makeView();
        view.forceActiveFocus();
        keyClick(Qt.Key_G, Qt.ControlModifier | Qt.ShiftModifier);
        const overlay = view.T.Overlay.overlay;
        const goal = findChild(overlay, "goalWords") as TextField;
        const deadline = findChild(overlay, "deadline") as TextField;
        tryVerify(() => goal && goal.visible);
        goal.forceActiveFocus();
        goal.selectAll();
        for (const c of "5000")
            keyClick(c);
        keyClick(Qt.Key_Tab);
        deadline.forceActiveFocus();
        for (const c of "soon")
            keyClick(c);
        keyClick(Qt.Key_Tab);
        const note = findChild(overlay, "dailyNeeded") as Text;
        verify(note.text.startsWith("Write the deadline"), note.text);
        deadline.selectAll();
        deadline.forceActiveFocus();
        const date = new Date(Date.now() + 4 * 86400000);
        deadline.text = "";
        for (const c of Qt.formatDate(date, "yyyy-MM-dd"))
            keyClick(c);
        keyClick(Qt.Key_Tab);
        compare(view.session.goalWords, 5000);
        verify(view.session.dailyNeeded > 0);
        verify(note.text.includes("words each day"), note.text);
    }

    function test_historyShowsTheHeatMap() {
        const view = makeView();
        view.forceActiveFocus();
        keyClick(Qt.Key_G, Qt.ControlModifier | Qt.ShiftModifier);
        const overlay = view.T.Overlay.overlay;
        const map = findChild(overlay, "heatMap") as Grid;
        tryVerify(() => map && map.visible);
        compare(map.children.length - 1, 182);
        const streak = findChild(overlay, "streak") as Text;
        compare(streak.text, "0 days in a row with words.");
    }

    function test_sprintCountsWordsUntilTheEnd() {
        const view = makeView();
        let scene = "";
        for (const item of view.session.items()) {
            if (item.kind === "scene" && scene === "")
                scene = item.id;
        }
        view.session.currentId = scene;
        view.startSprint(2);
        const status = findChild(view, "sprintStatus") as Text;
        verify(status.visible);
        verify(status.text.startsWith("Sprint 0:0"), status.text);
        const text = findChild(view, "editor") as TextEdit;
        text.text = text.text + " three new words";
        tryCompare(view, "sprintWords", 3, 2000);
        tryCompare(view, "sprintEnd", 0, 4000);
        compare(status.text, "Sprint: 3 words in 0 minutes");
    }

    function test_rightClickSuggestsAWord() {
        const view = makeView();
        compare(view.session.dictionary, "en_TEST");
        let scene = "";
        for (const item of view.session.items()) {
            if (item.kind === "scene" && scene === "")
                scene = item.id;
        }
        view.session.currentId = scene;
        const text = findChild(view, "editor") as TextEdit;
        text.text = "The lettr said";
        waitForRendering(view);
        const rect = text.positionToRectangle(6);
        mouseClick(text, rect.x + 1, rect.y + rect.height / 2, Qt.RightButton);
        const menu = findChild(view, "spellMenu") as Menu;
        tryVerify(() => menu.opened);
        keyClick(Qt.Key_Down);
        keyClick(Qt.Key_Return);
        compare(text.text.slice(0, 10), "The letter");
    }

    function test_turnOnVersions() {
        const view = makeView();
        verify(!view.session.versionsOn);
        view.forceActiveFocus();
        keyClick(Qt.Key_V, Qt.ControlModifier | Qt.AltModifier);
        verify(view.session.versionsOn);
        const message = findChild(view, "versionsMessage") as Text;
        compare(message.text, "Versions are on. The first version is saved.");
    }

    function test_saveAVersionAndOnClose() {
        const view = makeView();
        view.turnOnVersions();
        let scene = "";
        for (const item of view.session.items()) {
            if (item.kind === "scene" && scene === "")
                scene = item.id;
        }
        view.session.currentId = scene;
        (findChild(view, "editor") as TextEdit).text = "Changed text.";
        view.forceActiveFocus();
        keyClick(Qt.Key_S, Qt.ControlModifier | Qt.AltModifier);
        const overlay = view.T.Overlay.overlay;
        const field = findChild(overlay, "versionMessage") as TextField;
        tryVerify(() => field && field.activeFocus);
        compare(faults(field.parent), []);
        for (const c of "New ending")
            keyClick(c);
        keyClick(Qt.Key_Return);
        compare((findChild(view, "versionsMessage") as Text).text, "Version saved.");
        // Nothing changed since, so the close saves no empty version.
        view.versionOnClose();
        compare(view.session.saveVersion(""), "");
    }

    function test_versionsPanelListsTheVersions() {
        const view = makeView();
        view.forceActiveFocus();
        keyClick(Qt.Key_H, Qt.ControlModifier | Qt.AltModifier);
        verify(view.versionsOpen);
        const list = findChild(view, "versionList") as ListView;
        compare(list.count, 0);
        view.turnOnVersions();
        tryCompare(list, "count", 1);
        compare(list.model[0].message, "Versions on");
        let scene = "";
        for (const item of view.session.items()) {
            if (item.kind === "scene" && scene === "")
                scene = item.id;
        }
        view.session.currentId = scene;
        view.inspectorOpen = true;
        waitForRendering(view);
        const versions = findChild(view, "textVersions") as Item;
        verify(versions.visible);
        compare(faults(versions), []);
        keyClick(Qt.Key_Escape);
    }
}
