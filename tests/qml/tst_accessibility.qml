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
            compare(screenReader.role(dialog.contentItem.parent), Accessible.Dialog);
            verify(screenReader.name(dialog.contentItem.parent) !== "");
            compare(faults(dialog.contentItem), []);
            dialog.close();
        }
    }

    // A control with the keyboard focus shows a ring. A text field shows
    // its cursor in place of a ring, and a popup with no controls, such as
    // the key sheet, is itself the focus.
    function showsFocus(item: Item): bool {
        if (item instanceof TextInput || item instanceof TextEdit || String(item).startsWith("QQuickPopupItem"))
            return true;
        const walk = (node, depth) => {
            if (node instanceof FocusRing && node.visible)
                return true;
            if (depth === 0)
                return false;
            for (let i = 0; i < node.children.length; ++i) {
                if (walk(node.children[i], depth - 1))
                    return true;
            }
            return false;
        };
        return walk(item, 3);
    }

    // Tab from the focus through the controls until the focus comes back.
    // Returns the controls with no ring.
    function tabFaults(limit: int): list<string> {
        let found = [];
        const seen = [];
        for (let step = 0; step < limit; ++step) {
            keyClick(Qt.Key_Tab);
            const item = testCase.Window.window.activeFocusItem;
            // A text box of more than one line must not keep Tab. Only the
            // page of the editor does, for F6 moves on from it. A popup with
            // one stop, such as the palette, keeps the focus on purpose.
            if (item instanceof TextEdit && item === seen[seen.length - 1] && !(item instanceof PageText))
                found.push("Tab stuck: " + (item.objectName || item.Accessible.name || String(item)));
            if (!item || seen.includes(item))
                break;
            seen.push(item);
            if (!showsFocus(item))
                found.push("no ring: " + String(item).split("(")[0] + " " + (item.objectName || item.text || item.Accessible.name || ""));
        }
        return found;
    }

    function test_everyViewAndDialogWorksByKeyboard() {
        const view = makeView();
        let scene = "";
        for (const item of view.session.items()) {
            if (item.kind === "scene" && scene === "")
                scene = item.id;
        }
        view.session.currentId = scene;
        waitForRendering(view);
        const ids = ["board", "outline", "beats", "grid", "timeline", "insights", "family", "relations",
                     "calendar", "progress", "codex", "inbox", "checklist", "argument", "sources", "quotes", "submissions", "print", "export", "changes", "passes",
                     "versions.list", "help", "names", "palette", "search"];
        const panes = { "board": "corkboardView", "outline": "outlinerView", "beats": "beatBoard",
                        "grid": "plotGridView", "timeline": "timelineView", "insights": "insightsView",
                        "family": "familyTree", "relations": "relationGraph", "checklist": "checklistView", "argument": "argumentMap", "quotes": "quotesPanel",
                        "codex": "codexPanel", "inbox": "inboxPanel", "changes": "changesPanel",
                        "versions.list": "versionsPanel", "search": "searchPanel" };
        let all = [];
        for (const id of ids) {
            const command = view.commands.find(each => each.id === id);
            command.run();
            waitForRendering(view);
            wait(200);

            // Keys never reach the text that a view hides.
            if (view.viewMode !== "page")
                verify(!(findChild(view, "editor") as Item).activeFocus, id);
            const overlay = view.T.Overlay.overlay;
            const popup = overlay.children.some(child => child.visible);
            const root = popup ? overlay : view;
            // The screen reader gets a name and a role for the dialog or the view.
            const shown = popup ? overlay.children.find(child => child.visible && String(child).startsWith("QQuickPopupItem"))
                                : findChild(view, panes[id]) as Item;
            if (!shown || screenReader.name(shown) === "" || ![Accessible.Dialog, Accessible.Pane, Accessible.List].includes(screenReader.role(shown)))
                all.push(id + ": no name or role for the screen reader");
            all = all.concat(faults(root).map(f => id + ": " + f));
            all = all.concat(tabFaults(80).map(f => id + ": " + f));
            if (popup)
                keyClick(Qt.Key_Escape);
            else if (id !== "search")
                command.run();
            waitForRendering(view);
        }
        compare(all, []);
    }

    function test_returnOpensACardOfTheBoard() {
        const view = makeView();
        const chapter = view.session.items().find(item => item.kind === "chapter")?.id
                        ?? view.session.items()[0].id;
        view.session.currentId = chapter;
        view.toggleViewMode("board");
        waitForRendering(view);
        const board = findChild(view, "corkboardView") as Item;
        const find = item => {
            if (item instanceof Activator && item.visible)
                return item;
            for (let i = 0; i < item.children.length; ++i) {
                const found = find(item.children[i]);
                if (found)
                    return found;
            }
            return null;
        };
        const card = find(board);
        verify(card, "no card");
        // The board closes on Return and its cards go, so keep the name.
        const name = card.name;
        verify(name.startsWith("Open "));
        card.forceActiveFocus();
        verify(showsFocus(card));
        keyClick(Qt.Key_Return);
        compare(view.viewMode, "page");
        compare("Open " + view.session.fields(view.session.currentId).title, name);
    }

    function test_homeTabStartsWithTheTopBar() {
        const temp = StandardPaths.writableLocation(StandardPaths.TempLocation).toString().replace("file://", "");
        const folder = temp + "/codicier-home-" + Date.now();
        workspace.folder = folder;
        compare(workspace.createSample(), "");
        const win = Qt.createComponent("Codicier", "Main").createObject(null, { visible: true, width: 1200, height: 800 });
        Workspaces.open(folder);
        waitForRendering(win.contentItem);
        const names = [];
        for (let i = 0; i < 5; ++i) {
            keyClick(Qt.Key_Tab);
            const item = win.activeFocusItem;
            names.push(item ? (item.Accessible.name || item.text || "") : "");
        }
        compare(names.slice(0, 4), [Workspaces.currentName, "Import a project", "New project", "Settings"]);
        // The keys of the later tests go to the test window again.
        win.close();
        win.destroy();
        wait(50);
        testCase.Window.window.requestActivate();
        tryVerify(() => testCase.Window.window.active);
    }

    function test_longBackupPathStaysOnTheHomeScreen() {
        const temp = StandardPaths.writableLocation(StandardPaths.TempLocation).toString().replace("file://", "");
        const folder = temp + "/codicier-path-" + Date.now();
        workspace.folder = folder;
        compare(workspace.createSample(), "");
        const keep = Settings.backupFolder;
        Settings.backupFolder = temp + "/" + "a-very-long-folder-name-for-backups/".repeat(8);
        // One backup, so the line names the folder.
        const session = Qt.createQmlObject("import Codicier; ProjectSession {}", testCase);
        session.folder = workspace.get(0).folder;
        session.backupFolder = Settings.backupFolder;
        verify(session.setField(session.items().find(item => item.kind === "scene").id, "synopsis", "Changed."));
        verify(session.backup() !== "");
        session.destroy();
        wait(0);
        const win = Qt.createComponent("Codicier", "Main").createObject(null, { visible: true, width: 900, height: 700 });
        Workspaces.open(folder);
        waitForRendering(win.contentItem);
        tryVerify(() => (findChild(win.contentItem, "lastBackup") as Text)?.visible ?? false, 3000);
        const line = findChild(win.contentItem, "lastBackup") as Text;
        const fits = line.width <= line.parent.width + 1;
        const shortened = line.truncated;
        const width = line.width + " of " + line.parent.width;
        Settings.backupFolder = keep;
        verify(fits, "line " + width);
        verify(shortened);
        win.close();
        win.destroy();
        wait(50);
        testCase.Window.window.requestActivate();
        tryVerify(() => testCase.Window.window.active);
    }

    function test_oneImportMenuOnTheHomeScreen() {
        const temp = StandardPaths.writableLocation(StandardPaths.TempLocation).toString().replace("file://", "");
        const folder = temp + "/codicier-empty-" + Date.now();
        workspace.folder = folder;
        // A workspace folder that exists and holds no project.
        compare(workspace.create("Gone", "blank", ""), "");
        compare(workspace.remove(workspace.get(0).folder, ""), "");
        compare(workspace.count, 0);
        const win = Qt.createComponent("Codicier", "Main").createObject(null, { visible: true, width: 1000, height: 700 });
        Workspaces.open(folder);
        tryVerify(() => (findChild(win.contentItem, "emptyImport") as Item)?.visible ?? false, 3000);
        const overlay = win.contentItem.T.Overlay.overlay;
        // The open menu: a popup item on the overlay with the three imports.
        const openMenu = () => overlay.children.find(child => child.visible && String(child).startsWith("QQuickPopupItem"));
        const entries = item => {
            let found = [];
            const walk = node => {
                if (typeof node.text === "string" && node.text.endsWith("…") && !found.includes(node.text))
                    found.push(node.text);
                for (const child of node.children)
                    walk(child);
            };
            walk(item);
            return found;
        };
        waitForRendering(win.contentItem);
        mouseClick(findChild(win.contentItem, "emptyImport") as Item);
        tryVerify(() => openMenu() !== undefined);
        compare(entries(openMenu()), ["A Scrivener project…", "A folder of Markdown files…", "A Word file (DOCX)…", "A project zip…"]);
        keyClick(Qt.Key_Escape);
        tryVerify(() => openMenu() === undefined);
        mouseClick(findChild(win.contentItem, "importButton") as Item);
        tryVerify(() => openMenu() !== undefined);
        compare(entries(openMenu()).length, 4);
        keyClick(Qt.Key_Escape);
        win.close();
        win.destroy();
        wait(50);
        testCase.Window.window.requestActivate();
        tryVerify(() => testCase.Window.window.active);
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
        keyClick(Qt.Key_C, Qt.ControlModifier | Qt.ShiftModifier);
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

    function test_codexOffersNamesThatComeBack() {
        const view = makeView();
        const text = findChild(view, "editor") as TextEdit;
        text.text = "We waited for Quill. ".repeat(5);
        view.openCodex();
        verify(view.codexOpen);
        const box = findChild(view, "codexSuggestions") as Column;
        tryVerify(() => box.visible);
        const button = findChild(box, "codexSuggestion") as Button;
        compare(button.text, "Quill (5)");
        waitForRendering(view);
        mouseClick(button);
        verify(view.session.items().some(item => item.kind === "character" && item.title === "Quill"));
        tryVerify(() => !box.visible);
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

    function test_linkListStaysInsideTheEditor() {
        const view = makeView();
        view.session.rename(view.session.add("character"), "Mara Vell");
        view.session.rename(view.session.add("character"), "Abel Crane");
        view.session.rename(view.session.add("character"), "Mabel Moss");
        view.session.currentId = view.session.items().find(item => item.kind === "scene").id;
        const text = findChild(view, "editor") as TextEdit;
        text.text = Array(60).fill("A line of the scene.").join("\n\n");
        text.forceActiveFocus();
        text.cursorPosition = text.length;
        waitForRendering(view);
        for (const c of " [[a")
            keyClick(c);
        const list = findChild(view, "linkList") as Item;
        verify(list.visible);
        waitForRendering(view);
        const area = list.parent;
        verify(list.y >= 0, "top " + list.y);
        verify(list.y + list.height <= area.height, "bottom " + (list.y + list.height) + " of " + area.height);
        keyClick(Qt.Key_Escape);
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

    function test_dailyGoalShowsInTheStatusBar() {
        const view = makeView();
        const today = findChild(view, "todayCount") as Text;
        verify(!today.visible);
        verify(view.session.setDailyGoal(500));
        verify(today.visible);
        verify(today.text.startsWith("Today "), today.text);
        verify(today.text.includes(" of 500"), today.text);
        verify(view.session.setDailyGoal(0));
        verify(!today.visible);
        // A minute with an edit counts as writing time.
        const text = findChild(view, "editor") as TextEdit;
        text.forceActiveFocus();
        for (const c of "dusk")
            keyClick(c);
        tryVerify(() => view.session.todayMinutes >= 1, 2000);
    }

    function test_sessionChartShowsClosedSessions() {
        const view = makeView();
        const text = findChild(view, "editor") as TextEdit;
        text.text = "Four words for now.";
        tryVerify(() => view.session.sessionWords >= 4, 2000);
        view.session.backup();
        view.forceActiveFocus();
        keyClick(Qt.Key_G, Qt.ControlModifier | Qt.ShiftModifier);
        const chart = findChild(view.T.Overlay.overlay, "sessionChart") as Item;
        tryVerify(() => chart && chart.visible);
        compare(chart.sessions.length, 1);
        compare(faults(chart), []);
        keyClick(Qt.Key_Escape);
    }

    function test_heatMapColoursStayValid() {
        failOnWarning(/setAlphaF/);
        const view = makeView();
        const text = findChild(view, "editor") as TextEdit;
        text.forceActiveFocus();
        for (const c of "the lamp burned on")
            keyClick(c);
        // The history counts the words after the autosave.
        wait(1500);
        view.forceActiveFocus();
        keyClick(Qt.Key_G, Qt.ControlModifier | Qt.ShiftModifier);
        const map = findChild(view.T.Overlay.overlay, "heatMap") as Grid;
        tryVerify(() => map && map.visible);
        waitForRendering(map);
        keyClick(Qt.Key_Escape);
        tryVerify(() => !map.visible);
    }

    SignalSpy {
        id: finishedSpy

        signalName: "sprintFinished"
    }

    function test_sprintCountsWordsUntilTheEnd() {
        const view = makeView();
        let scene = "";
        for (const item of view.session.items()) {
            if (item.kind === "scene" && scene === "")
                scene = item.id;
        }
        view.session.currentId = scene;
        finishedSpy.target = view;
        finishedSpy.clear();
        view.startSprint(2);
        const status = findChild(view, "sprintStatus") as Text;
        verify(status.visible);
        verify(status.text.startsWith("Sprint 0:0"), status.text);
        const text = findChild(view, "editor") as TextEdit;
        text.text = text.text + " three new words";
        tryCompare(view, "sprintWords", 3, 2000);
        tryCompare(view, "sprintEnd", 0, 4000);
        compare(status.text, "Sprint: 3 words in 0 minutes");
        // The end of the time tells the author.
        compare(finishedSpy.count, 1);
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

    function test_versionsPanelTurnsVersionsOn() {
        const view = makeView();
        verify(!view.session.versionsOn);
        view.commands.find(command => command.id === "versions.list").run();
        const panel = findChild(view, "versionsPanel") as Item;
        tryVerify(() => panel.visible);
        const button = findChild(panel, "turnOnVersions") as Item;
        verify(button.visible);
        mouseClick(button);
        tryVerify(() => view.session.versionsOn, 5000);
        verify(!button.visible);
        compare(view.session.versions("").length, 1);
    }

    function test_turnOnVersions() {
        const view = makeView();
        verify(!view.session.versionsOn);
        view.forceActiveFocus();
        view.commands.find(command => command.id === "versions.on").run();
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
        view.commands.find(command => command.id === "versions.list").run();
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
        // The button can sit below the visible part of the inspector.
        (findChild(versions, "compareVersion") as Button).clicked();
        verify(view.comparePath.startsWith("version:"));
        keyClick(Qt.Key_Escape);
    }

    function test_beatBoardDragPutsASceneOnABeat() {
        const view = makeView();
        view.session.setFramework("kishotenketsu");
        view.forceActiveFocus();
        view.commands.find(command => command.id === "beats").run();
        compare(view.viewMode, "beats");
        const list = findChild(view, "beatColumns") as ListView;
        tryCompare(list, "count", 5);
        waitForRendering(view);
        compare(faults(findChild(view, "beatBoard") as Item), []);
        const card = findChild(list.itemAtIndex(0), "beatScene") as Item;
        verify(card);
        const target = list.itemAtIndex(2);
        const from = card.mapToItem(view, card.width / 2, card.height / 2);
        const to = target.mapToItem(view, target.width / 2, target.height / 2);
        mousePress(view, from.x, from.y);
        for (let i = 1; i <= 10; ++i)
            mouseMove(view, from.x + (to.x - from.x) * i / 10, from.y + (to.y - from.y) * i / 10);
        mouseRelease(view, to.x, to.y);
        const columns = view.session.beatBoard();
        compare(columns[2].scenes.length, 1);
        compare(columns[2].id, "sho");
    }

    function test_inspectorAddsPlotlines() {
        const view = makeView();
        let scene = "";
        for (const item of view.session.items()) {
            if (item.kind === "scene" && scene === "")
                scene = item.id;
        }
        view.session.currentId = scene;
        view.inspectorOpen = true;
        waitForRendering(view);
        const field = findChild(view, "newPlotline") as TextField;
        field.forceActiveFocus();
        for (const c of "Main")
            keyClick(c);
        keyClick(Qt.Key_Return);
        compare(view.session.fields(scene).plotlines, ["main"]);
        compare(faults(findChild(view, "plotlineSection") as Item), []);
        (findChild(view, "plotline-main") as Choice).clicked();
        compare(view.session.fields(scene).plotlines, []);
    }

    function test_plotGridClicksAndDrags() {
        const view = makeView();
        const a = view.session.addPlotline("A");
        const b = view.session.addPlotline("B");
        const scene = view.session.plotGrid()[0].id;
        view.forceActiveFocus();
        view.commands.find(command => command.id === "grid").run();
        compare(view.viewMode, "grid");
        waitForRendering(view);
        const gridView = findChild(view, "plotGridView") as Item;
        compare(faults(gridView), []);
        // Each plotline has its own colour.
        const rows = [];
        const collect = item => {
            if (item.objectName === "plotRow")
                rows.push(item);
            for (const child of item.children)
                collect(child);
        };
        collect(gridView);
        compare(rows.length, 2);
        verify(!Qt.colorEqual(rows[0].lineColor, rows[1].lineColor));
        // The first cell of the first row: scene one on plotline A.
        const cell = findChild(findChild(gridView, "plotRow") as Item, "plotCell") as Item;
        mouseClick(cell);
        compare(view.session.fields(scene).plotlines, [a]);
        waitForRendering(view);
        // Drag the mark one row down, onto plotline B.
        const mark = findChild(findChild(gridView, "plotRow") as Item, "plotCell") as Item;
        const from = mark.mapToItem(view, mark.width / 2, mark.height / 2);
        mousePress(view, from.x, from.y);
        for (let i = 1; i <= 10; ++i)
            mouseMove(view, from.x, from.y + 4 * i);
        mouseRelease(view, from.x, from.y + 40);
        compare(view.session.fields(scene).plotlines, [b]);
    }

    function test_timelinePlacesEventsByDate() {
        const view = makeView();
        const scenes = view.session.plotGrid();
        view.session.setField(scenes[0].id, "date", "1847-03-04");
        view.session.setField(scenes[0].id, "pov", "Mara");
        const old = view.session.add("character");
        view.session.rename(old, "Old Vell");
        view.session.setField(old, "date", "1801-06-12");
        view.forceActiveFocus();
        view.commands.find(command => command.id === "timeline").run();
        compare(view.viewMode, "timeline");
        const timelineView = findChild(view, "timelineView") as Timeline;
        compare(timelineView.tracks.length, 2);
        waitForRendering(view);
        const tracks = findChild(view, "timelineTracks") as Flickable;
        const events = [];
        const walk = item => {
            for (let i = 0; i < item.children.length; ++i) {
                const child = item.children[i];
                if (child.objectName === "timelineEvent")
                    events.push(child);
                walk(child);
            }
        };
        walk(tracks.contentItem);
        compare(events.length, 2);
        // The older event is on the left, at the start of the time line.
        events.sort((a, b) => a.x - b.x);
        compare(events[0].Accessible.name, "Old Vell, 12 June 1801");
        compare(events[0].x, timelineView.nameWidth);
        verify(events[1].x > events[0].x);
        verify(events[1].x <= timelineView.nameWidth + timelineView.usable);
        compare(faults(timelineView), []);
        // Each lane has a band, and the lanes alternate.
        const bands = [];
        const collect = item => {
            if (item.objectName === "trackBand")
                bands.push(item);
            for (const child of item.children)
                collect(child);
        };
        collect(tracks.contentItem);
        compare(bands.length, 2);
        verify(!Qt.colorEqual(bands[0].color, bands[1].color));
        mouseClick(events[1]);
        compare(view.session.currentId, scenes[0].id);
        compare(view.viewMode, "page");
    }

    function test_timelineCardsDoNotOverlap() {
        const view = makeView();
        while (view.session.plotGrid().length < 4)
            view.session.add("scene");
        const scenes = view.session.plotGrid();
        const dates = ["1912-11-02", "1912-11-02", "1912-11-03", "1930-01-01"];
        for (let i = 0; i < 4; ++i) {
            view.session.setField(scenes[i].id, "date", dates[i]);
            view.session.setField(scenes[i].id, "pov", "Elin");
        }
        view.forceActiveFocus();
        view.commands.find(command => command.id === "timeline").run();
        const timelineView = findChild(view, "timelineView") as Timeline;
        compare(timelineView.tracks.length, 1);
        waitForRendering(view);
        const tracks = findChild(view, "timelineTracks") as Flickable;
        const events = [];
        const walk = item => {
            for (let i = 0; i < item.children.length; ++i) {
                const child = item.children[i];
                if (child.objectName === "timelineEvent")
                    events.push(child);
                walk(child);
            }
        };
        walk(tracks.contentItem);
        compare(events.length, 4);
        for (let i = 0; i < events.length; ++i) {
            for (let j = i + 1; j < events.length; ++j) {
                const a = events[i].mapToItem(tracks.contentItem, 0, 0);
                const b = events[j].mapToItem(tracks.contentItem, 0, 0);
                const apart = a.x + events[i].width <= b.x || b.x + events[j].width <= a.x
                              || a.y + events[i].height <= b.y || b.y + events[j].height <= a.y;
                verify(apart, events[i].Accessible.name + " covers " + events[j].Accessible.name);
            }
        }
        // Each card is inside the band of its own lane, and lanes alternate.
        for (const event of events) {
            let lane = event.parent;
            while (lane && lane.objectName !== "timelineTrack")
                lane = lane.parent;
            verify(lane, "no lane for " + event.Accessible.name);
            const at = event.mapToItem(lane, 0, 0);
            verify(at.y >= 0 && at.y + event.height <= lane.height, event.Accessible.name + " leaves its lane");
        }
    }

    function test_calendarDialogSetsTheMonths() {
        const view = makeView();
        view.forceActiveFocus();
        view.commands.find(command => command.id === "calendar").run();
        const overlay = view.T.Overlay.overlay;
        const field = findChild(overlay, "calendarMonths") as TextArea;
        tryVerify(() => field && field.visible);
        compare(faults(field.parent), []);
        field.text = "Frostmonth\nThaw 30";
        mouseClick(findChild(overlay, "saveCalendar") as Item);
        compare(view.session.calendarMonths().length, 0);
        field.text = "Frostmonth 40\nThaw 30";
        mouseClick(findChild(overlay, "saveCalendar") as Item);
        compare(view.session.calendarMonths().length, 2);
        compare(view.session.calendarMonths()[0].name, "Frostmonth");
        view.commands.find(command => command.id === "calendar").run();
        tryVerify(() => field.visible);
        compare(field.text, "Frostmonth 40\nThaw 30");
        mouseClick(findChild(overlay, "gregorian") as Item);
        compare(view.session.calendarMonths().length, 0);
    }

    function test_inspectorListsRelations() {
        const view = makeView();
        const mara = view.session.add("character");
        view.session.rename(mara, "Mara");
        const ilse = view.session.add("character");
        view.session.rename(ilse, "Ilse");
        view.session.addRelation(mara, ilse, "daughter of", false);
        view.session.currentId = mara;
        view.inspectorOpen = true;
        waitForRendering(view);
        const section = findChild(view, "relationSection") as Item;
        verify(section.visible);
        compare(faults(section), []);
        let button = null;
        const walk = item => {
            for (let i = 0; i < item.children.length; ++i) {
                const child = item.children[i];
                if (child instanceof IconButton && child.label.startsWith("Remove the relation"))
                    button = child;
                walk(child);
            }
        };
        walk(section);
        verify(button !== null);
        button.clicked();
        compare(view.session.relations(mara).length, 0);
    }

    function test_relationGraphHintsWithNoRelation() {
        const view = makeView();
        view.session.rename(view.session.add("character"), "Mara");
        view.session.rename(view.session.add("character"), "Abel");
        view.commands.find(command => command.id === "relations").run();
        const graphView = findChild(view, "relationGraph") as RelationGraph;
        waitForRendering(view);
        const hint = findChild(graphView, "graphHint") as Text;
        verify(hint.visible);
        verify(hint.text.startsWith("No relations yet."));
        // Two nodes stay close: the circle is small.
        verify(graphView.radius <= 120, "radius " + graphView.radius);
        const ids = graphView.nodes.map(node => node.id);
        view.session.addRelation(ids[0], ids[1], "knows", false);
        tryVerify(() => !hint.visible);
    }

    function test_relationGraphShowsEntities() {
        const view = makeView();
        const mara = view.session.add("character");
        view.session.rename(mara, "Mara");
        const port = view.session.add("place");
        view.session.rename(port, "Port");
        const lone = view.session.add("place");
        view.session.addRelation(mara, port, "lives in", false);
        view.forceActiveFocus();
        view.commands.find(command => command.id === "relations").run();
        compare(view.viewMode, "relations");
        const graphView = findChild(view, "relationGraph") as RelationGraph;
        // The character, and the place with a relation, but not the lone place.
        compare(graphView.nodes.map(node => node.id).sort(), [mara, port].sort());
        waitForRendering(view);
        compare(faults(graphView), []);
        const node = findChild(graphView, "graphNode") as Item;
        mouseClick(node);
        verify(view.session.currentId === mara || view.session.currentId === port);
        compare(view.viewMode, "page");
    }

    function test_familyTreeShowsGenerations() {
        const view = makeView();
        const ilse = view.session.add("character");
        view.session.rename(ilse, "Ilse");
        const mara = view.session.add("character");
        view.session.rename(mara, "Mara");
        view.session.addRelation(ilse, mara, "", true);
        view.forceActiveFocus();
        view.commands.find(command => command.id === "family").run();
        compare(view.viewMode, "family");
        const treeView = findChild(view, "familyTree") as FamilyTree;
        compare(treeView.generations.length, 2);
        waitForRendering(view);
        compare(faults(treeView), []);
        mouseClick(findChild(treeView, "treePerson") as Item);
        compare(view.viewMode, "page");
    }

    function test_relationshipGraphSwitchesToTheFamilyLayout() {
        const view = makeView();
        view.forceActiveFocus();
        view.commands.find(command => command.id === "relations").run();
        compare(view.viewMode, "relations");
        const graph = findChild(view, "relationGraph") as RelationGraph;
        compare(graph.layout, "circle");
        verify(!(findChild(graph, "familyTree") as FamilyTree).visible);
        waitForRendering(view);
        mouseClick(findChild(graph, "layoutFamily") as Choice);
        compare(view.viewMode, "family");
        compare(graph.layout, "family");
        verify((findChild(graph, "familyTree") as FamilyTree).visible);
        mouseClick(findChild(graph, "layoutCircle") as Choice);
        compare(view.viewMode, "relations");
    }

    function test_mapShowsItsImage() {
        const view = makeView();
        compare(view.session.addMap("file:///no/such/map.png"), "");
        const map = view.session.addMap("file://" + Qt.resolvedUrl("../fixtures/map.png").toString().replace("file://", ""));
        verify(map !== "");
        compare(view.session.currentId, map);
        compare(view.currentKind, "map");
        const mapView = findChild(view, "mapView") as MapView;
        verify(mapView.visible);
        const image = findChild(mapView, "mapImage") as Image;
        tryCompare(image, "status", Image.Ready);
        compare(image.sourceSize.width, 8);
    }

    function test_mapPinsOpenAndMove() {
        const view = makeView();
        const port = view.session.add("place");
        view.session.rename(port, "Port");
        const map = view.session.addMap("file://" + Qt.resolvedUrl("../fixtures/map.png").toString().replace("file://", ""));
        verify(view.session.addPin(map, port, 0.5, 0.5));
        view.session.currentId = map;
        const mapView = findChild(view, "mapView") as MapView;
        tryCompare(findChild(mapView, "mapImage") as Image, "status", Image.Ready);
        waitForRendering(view);
        compare(faults(mapView), []);
        const pin = findChild(mapView, "mapPin") as Item;
        verify(pin);
        // A drag to the right moves the pin.
        const from = pin.mapToItem(view, pin.width / 2, 12);
        mousePress(view, from.x, from.y);
        for (let i = 1; i <= 10; ++i)
            mouseMove(view, from.x + 4 * i, from.y);
        mouseRelease(view, from.x + 40, from.y);
        verify(view.session.pins(map)[0].x > 0.5);
        // A click opens the place.
        const moved = findChild(mapView, "mapPin") as Item;
        mouseClick(moved, moved.width / 2, 12);
        compare(view.session.currentId, port);
    }

    function test_tensionRatingDrawsTheLineInTheInsights() {
        const view = makeView();
        const scene = view.session.plotGrid()[0].id;
        view.session.currentId = scene;
        view.inspectorOpen = true;
        waitForRendering(view);
        const four = findChild(view, "tension4") as Choice;
        verify(four.visible);
        mouseClick(four);
        compare(view.session.fields(scene).tension, 4);
        view.commands.find(command => command.id === "insights").run();
        const insightsView = findChild(view, "insightsView") as Insights;
        insightsView.refresh();
        waitForRendering(view);
        verify((findChild(insightsView, "tensionChart") as Item).visible);
        compare(insightsView.figures.tension.length, 1);
    }

    function test_insightsShowTheLengths() {
        failOnWarning(/TypeError/);
        const view = makeView();
        view.session.currentId = view.session.plotGrid()[0].id;
        (findChild(view, "editor") as TextEdit).text = "The harbour slept under the snow.";
        // The live count arrives a moment after the edit.
        tryCompare(view.session, "sceneWords", 6);
        view.forceActiveFocus();
        view.commands.find(command => command.id === "insights").run();
        compare(view.viewMode, "insights");
        const insightsView = findChild(view, "insightsView") as Insights;
        verify(insightsView.figures.chapters.length > 0);
        waitForRendering(view);
        compare(faults(insightsView), []);
        verify(findChild(insightsView, "lengthChapter"));
        verify(findChild(insightsView, "readability"));
        compare((findChild(insightsView, "bookReading") as Text).text,
                "The manuscript: 6 words, under 1 min to read.");
        verify(findChild(insightsView, "povChapter"));
        verify(findChild(insightsView, "dialogueChapter"));
        verify(findChild(insightsView, "frequentWord"));
        const grid = findChild(insightsView, "presenceGrid") as Grid;
        verify(!grid.visible);
        const harbour = view.session.add("character");
        view.session.rename(harbour, "Harbour");
        insightsView.refresh();
        waitForRendering(view);
        verify(grid.visible);
        const cell = findChild(grid, "presenceCell") as Item;
        verify(cell.Accessible.name.startsWith("Harbour in "), cell.Accessible.name);
        compare(faults(grid), []);
    }

    function test_passDialogStartsAPass() {
        const view = makeView();
        view.forceActiveFocus();
        view.commands.find(command => command.id === "passes").run();
        const overlay = view.T.Overlay.overlay;
        const field = findChild(overlay, "passName") as TextField;
        tryVerify(() => field && field.activeFocus);
        compare(faults(field.parent.parent), []);
        for (const c of "Structure")
            keyClick(c);
        keyClick(Qt.Key_Return);
        compare(view.session.passes().length, 1);
        verify(view.session.showPasses);
        compare(field.text, "");
    }

    function test_commentOnSelectionAndInbox() {
        const view = makeView();
        const scene = view.session.plotGrid()[0].id;
        view.session.currentId = scene;
        const text = findChild(view, "editor") as TextEdit;
        text.text = "The tide came in fast.";
        text.forceActiveFocus();
        text.select(4, 8);
        keyClick(Qt.Key_K, Qt.ControlModifier | Qt.AltModifier);
        const overlay = view.T.Overlay.overlay;
        const field = findChild(overlay, "commentText") as TextArea;
        tryVerify(() => field && field.activeFocus);
        compare(faults(field.parent), []);
        for (const c of "Which tide")
            keyClick(c);
        mouseClick(findChild(overlay, "saveComment") as Item);
        compare(view.session.comments(scene)[0].quote, "tide");

        view.forceActiveFocus();
        keyClick(Qt.Key_O, Qt.ControlModifier | Qt.AltModifier);
        verify(view.inboxOpen);
        const list = findChild(view, "commentList") as ListView;
        tryCompare(list, "count", 1);
        waitForRendering(view);
        compare(faults(findChild(view, "inboxPanel") as Item), []);
        view.session.currentId = "";
        mouseClick(list.itemAtIndex(0), 10, 10);
        compare(view.session.currentId, scene);
        compare(text.selectedText, "tide");
        (findChild(list.itemAtIndex(0), "resolveComment") as Button).clicked();
        tryCompare(list, "count", 0);
    }

    function test_checklistTicksAChapter() {
        const view = makeView();
        view.session.startPass("Structure");
        view.forceActiveFocus();
        view.commands.find(command => command.id === "checklist").run();
        compare(view.viewMode, "checklist");
        const listView = findChild(view, "checklistView") as Item;
        const field = findChild(listView, "newCheck") as TextField;
        field.forceActiveFocus();
        for (const c of "Cut adverbs")
            keyClick(c);
        keyClick(Qt.Key_Return);
        compare(view.session.checklist(1).checks, ["Cut adverbs"]);
        waitForRendering(view);
        compare(faults(listView), []);
        // The first visible tick box: the name cell of a row has a hidden one.
        let tick = null;
        const walk = item => {
            for (let i = 0; i < item.children.length; ++i) {
                const child = item.children[i];
                if (!tick && child.objectName === "tick" && child.visible)
                    tick = child;
                walk(child);
            }
        };
        walk(listView);
        verify(tick !== null);
        mouseClick(tick);
        verify(view.session.checklist(1).chapters[0].ticked[0]);
    }
}
