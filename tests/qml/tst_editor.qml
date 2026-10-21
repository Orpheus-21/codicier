pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Templates as T
import QtTest
import Codicier

TestCase {
    id: testCase

    name: "Editor"
    when: windowShown
    visible: true
    width: 900
    height: 700

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
        workspace.folder = temp + "/codicier-qml-" + Date.now() + "-" + Math.floor(Math.random() * 1e6);
        compare(workspace.create("Harbour", "short-story", ""), "");
        const view = createTemporaryObject(viewComponent, testCase, { folder: workspace.folder + "/harbour" }) as ProjectView;
        compare(view.session.error, "");
        return view;
    }

    function cleanup() {
        Settings.theme = "system";
    }

    function test_newNovelOpensOnItsFirstScene() {
        const temp = StandardPaths.writableLocation(StandardPaths.TempLocation).toString().replace("file://", "");
        workspace.folder = temp + "/codicier-qml-" + Date.now() + "-" + Math.floor(Math.random() * 1e6);
        compare(workspace.create("Lamp", "novel", ""), "");
        const view = createTemporaryObject(viewComponent, testCase, { folder: workspace.folder + "/lamp" }) as ProjectView;
        compare(view.session.fields(view.session.currentId).title, "Scene 1");
        const text = findChild(view, "editor") as TextEdit;
        tryVerify(() => text.activeFocus);
        const hint = findChild(view, "emptyHint") as Text;
        verify(hint.visible);
        for (const c of "Dusk")
            keyClick(c);
        compare(text.text, "Dusk");
        verify(!hint.visible);
    }

    function test_boardAndOutlinerShowTheScenesOfTheChapter() {
        const temp = StandardPaths.writableLocation(StandardPaths.TempLocation).toString().replace("file://", "");
        workspace.folder = temp + "/codicier-qml-" + Date.now() + "-" + Math.floor(Math.random() * 1e6);
        compare(workspace.create("Lamp", "novel", ""), "");
        const view = createTemporaryObject(viewComponent, testCase, { folder: workspace.folder + "/lamp" }) as ProjectView;
        const first = view.session.currentId;
        verify(view.session.add("scene") !== "");
        view.session.currentId = first;
        view.viewMode = "board";
        tryCompare(findChild(view, "corkboard") as GridView, "count", 2);
        view.viewMode = "outline";
        tryCompare(findChild(view, "outlinerRows") as ListView, "count", 2);
    }

    function test_boardFollowsTheLiveWordCount() {
        const temp = StandardPaths.writableLocation(StandardPaths.TempLocation).toString().replace("file://", "");
        workspace.folder = temp + "/codicier-qml-" + Date.now() + "-" + Math.floor(Math.random() * 1e6);
        compare(workspace.create("Lamp", "novel", ""), "");
        const view = createTemporaryObject(viewComponent, testCase, { folder: workspace.folder + "/lamp" }) as ProjectView;
        const text = findChild(view, "editor") as TextEdit;
        tryVerify(() => text.activeFocus);
        for (const c of "one two three")
            keyClick(c);
        // The board opens before the live count arrives.
        view.viewMode = "board";
        const board = findChild(view, "corkboardView") as Corkboard;
        tryVerify(() => board.cards.length > 0 && board.cards[0].words === 3);
        view.viewMode = "outline";
        const outliner = findChild(view, "outlinerView") as Outliner;
        tryVerify(() => outliner.rows.length > 0 && outliner.rows[0].words === 3);
    }

    SignalSpy {
        id: closeSpy

        signalName: "closeRequested"
    }

    function test_backButtonClosesTheProject() {
        const view = makeView();
        closeSpy.target = view;
        closeSpy.clear();
        const back = findChild(view, "backToWorkspace") as IconButton;
        verify(back && back.visible);
        compare(back.keys, "Ctrl+W");
        mouseClick(back);
        compare(closeSpy.count, 1);
    }

    function test_f1ListsEveryCommandWithItsKey() {
        const view = makeView();
        view.forceActiveFocus();
        keyClick(Qt.Key_F1);
        const sheet = findChild(view, "keySheet") as KeySheet;
        tryVerify(() => sheet.opened);
        let rows = 0;
        for (const group of sheet.groups)
            rows += group.rows.length;
        compare(rows, view.commands.length);
        verify(sheet.groups.some(group => group.rows.some(row => row.id === "help" && row.sequence === "F1")));
        keyClick(Qt.Key_Escape);
        tryVerify(() => !sheet.visible);
    }

    function test_firstRunHintsStepWithEscape() {
        Settings.hintsSeen = false;
        const view = makeView();
        const hint = findChild(view, "firstRunHint") as FirstRunHint;
        verify(hint.visible);
        compare(hint.step, 0);
        keyClick(Qt.Key_Escape);
        compare(hint.step, 1);
        mouseClick(hint);
        compare(hint.step, 2);
        verify(hint.steps[2].text.includes("Ctrl+Shift+P"));
        keyClick(Qt.Key_Escape);
        verify(!hint.visible);
        verify(Settings.hintsSeen);
        // The next project starts with no hints.
        const again = makeView();
        verify(!(findChild(again, "firstRunHint") as FirstRunHint).visible);
    }

    function test_boldAndItalicToggleTheMarks() {
        const view = makeView();
        const text = findChild(view, "editor") as TextEdit;
        text.text = "dusk fell";
        text.forceActiveFocus();
        text.select(0, 4);
        keyClick(Qt.Key_I, Qt.ControlModifier);
        compare(text.text, "*dusk* fell");
        compare(text.selectedText, "dusk");
        keyClick(Qt.Key_I, Qt.ControlModifier);
        compare(text.text, "dusk fell");
        // With no selection, the word at the cursor gets the mark.
        text.cursorPosition = 7;
        keyClick(Qt.Key_B, Qt.ControlModifier);
        compare(text.text, "dusk **fell**");
        // A selection with its marks inside loses them too.
        text.select(5, 13);
        keyClick(Qt.Key_B, Qt.ControlModifier);
        compare(text.text, "dusk fell");
        // Italic inside bold: three stars, and italic comes off alone.
        text.text = "***dusk***";
        text.select(3, 7);
        keyClick(Qt.Key_I, Qt.ControlModifier);
        compare(text.text, "**dusk**");
        // Outside a word, the pair goes in with the cursor between.
        text.text = "dusk ";
        text.cursorPosition = 5;
        keyClick(Qt.Key_B, Qt.ControlModifier);
        compare(text.text, "dusk ****");
        compare(text.cursorPosition, 7);
        // The inspector moved to Ctrl+Shift+I.
        compare(view.keys.sequences["inspector"], "Ctrl+Shift+I");
    }

    function test_findInTheSceneStepsAndReplaces() {
        const view = makeView();
        const text = findChild(view, "editor") as TextEdit;
        text.text = "The gull, the Gull, the gulls.";
        text.forceActiveFocus();
        text.cursorPosition = 0;
        keyClick(Qt.Key_F, Qt.ControlModifier);
        const bar = findChild(view, "findBar") as FindBar;
        const field = findChild(bar, "findField") as TextField;
        verify(bar.visible);
        verify(field.activeFocus);
        for (const c of "gull")
            keyClick(c);
        compare(bar.hits.length, 3);
        compare(text.selectionStart, 4);
        keyClick(Qt.Key_Return);
        compare(text.selectionStart, 14);
        keyClick(Qt.Key_Return, Qt.ShiftModifier);
        compare(text.selectionStart, 4);
        compare((findChild(bar, "findCount") as Text).text, "1 of 3");
        keyClick(Qt.Key_Escape);
        verify(!bar.visible);
        verify(text.activeFocus);

        // Ctrl+R opens the replace row. All replaces every match.
        keyClick(Qt.Key_R, Qt.ControlModifier);
        verify(bar.replacing);
        (findChild(bar, "replaceField") as TextField).text = "tern";
        waitForRendering(view);
        mouseClick(findChild(bar, "replaceAll") as Item);
        compare(text.text, "The tern, the tern, the terns.");
        compare(bar.hits.length, 0);
    }

    function test_typographyChangesAsYouType() {
        const view = makeView();
        const text = findChild(view, "editor") as TextEdit;
        text.text = "";
        text.forceActiveFocus();
        const typeOut = keys => {
            for (const c of keys)
                keyClick(c);
            wait(0);
        };
        typeOut("\"Wait,\" she said. It's here -- now...");
        tryCompare(text, "text", "\u201CWait,\u201D she said. It\u2019s here \u2014 now\u2026");
        // One undo gives back the typed dots.
        keyClick(Qt.Key_Z, Qt.ControlModifier);
        compare(text.text, "\u201CWait,\u201D she said. It\u2019s here \u2014 now...");

        // A switch that is off leaves the quotes straight.
        Settings.typography = 6;
        text.text = "";
        typeOut("'a'");
        compare(text.text, "'a'");
        Settings.typography = 7;
    }

    function test_statusBarCountsTheSelection() {
        const view = makeView();
        const text = findChild(view, "editor") as TextEdit;
        text.text = "The gull cried. The tide turned.";
        const count = findChild(view, "selectionCount") as Text;
        verify(!count.visible);
        // "gull cried. The tide " holds four words.
        text.select(4, 25);
        verify(count.visible);
        compare(count.text, "Selection 4");
    }

    function test_statusBarShowsTheSaveState() {
        const view = makeView();
        const state = findChild(view, "saveState") as Text;
        compare(state.text, "Saved");
        (findChild(view, "editor") as TextEdit).text = "The gull cried.";
        compare(state.text, "Saving");
        tryCompare(state, "text", "Saved", 5000);
    }

    function test_themeColoursReachTheDocument() {
        const view = makeView();
        const document = view.session.document;
        Settings.theme = "dark";
        verify(Qt.colorEqual(document.markColor, Theme.palettes.dark.mark));
        verify(Qt.colorEqual(document.accentColor, Theme.palettes.dark.accent));
        Settings.theme = "sepia";
        verify(Qt.colorEqual(document.markColor, Theme.palettes.sepia.mark));
        verify(document.lineDistance >= 0);
    }

    function test_f11EntersAndEscapeLeavesFocusMode() {
        const view = makeView();
        waitForRendering(view);
        view.forceActiveFocus();
        keyClick(Qt.Key_F11);
        verify(view.focusMode);
        keyClick(Qt.Key_Escape);
        verify(!view.focusMode);
        keyClick(Qt.Key_F11);
        keyClick(Qt.Key_F11);
        verify(!view.focusMode);
    }

    function test_typewriterKeepsTheCursorLineAtOneHeight() {
        const view = makeView();
        const model = view.session.binder;
        view.session.currentId = model.data(model.index(1, 0), BinderModel.IdRole);
        const text = findChild(view, "editor") as TextEdit;
        const page = findChild(view, "page") as Flickable;
        verify(text && page);
        let lines = [];
        for (let i = 0; i < 200; ++i)
            lines.push("Line " + i + " of a long scene.");
        text.text = lines.join("\n\n");
        view.setFocusMode(true);
        waitForRendering(view);
        text.cursorPosition = Math.floor(text.length / 2);
        // The cursor line sits at 45% of the page height, after the scroll.
        tryVerify(() => Math.abs(text.cursorRectangle.y + Theme.space.xxxl - page.contentY
                                 - page.height * 0.45) < 2, 2000);
        view.setFocusMode(false);
    }

    function test_ctrlKSplitsAtTheCursor() {
        const view = makeView();
        const model = view.session.binder;
        const first = model.data(model.index(1, 0), BinderModel.IdRole);
        view.session.currentId = first;
        const text = findChild(view, "editor") as TextEdit;
        text.forceActiveFocus();
        for (const c of "One")
            keyClick(c);
        keyClick(Qt.Key_Return);
        keyClick(Qt.Key_Return);
        for (const c of "Two")
            keyClick(c);
        compare(view.session.document.dirty, true);
        text.cursorPosition = text.text.indexOf("Two");
        keyClick(Qt.Key_K, Qt.ControlModifier);
        verify(view.session.currentId !== first);
        compare(model.data(model.index(2, 0), BinderModel.TitleRole), "Opening (2)");
        compare(text.text, "Two");
    }

    function test_searchOpensTheMatch() {
        const view = makeView();
        const model = view.session.binder;
        const first = model.data(model.index(1, 0), BinderModel.IdRole);
        view.session.currentId = first;
        const text = findChild(view, "editor") as TextEdit;
        text.text = "The tide came in. A gull cried over the Harbour wall.";
        view.session.currentId = "";
        waitForRendering(view);

        view.forceActiveFocus();
        keyClick(Qt.Key_F, Qt.ControlModifier | Qt.ShiftModifier);
        verify(view.searchOpen);
        for (const c of "GULL")
            keyClick(c);
        const results = findChild(view, "searchResults") as ListView;
        tryCompare(results, "count", 1);
        keyClick(Qt.Key_Return);
        compare(view.session.currentId, first);
        compare(text.selectedText, "gull");
        verify(text.activeFocus);

        // Alt+R makes the query a pattern. A bad pattern shows the reason.
        keyClick(Qt.Key_F, Qt.ControlModifier | Qt.ShiftModifier);
        keyClick(Qt.Key_R, Qt.AltModifier);
        for (const c of "g(ul")
            keyClick(c);
        const error = findChild(view, "searchError") as Text;
        verify(error.visible);
        verify(error.text !== "");
        for (let i = 0; i < 3; ++i)
            keyClick(Qt.Key_Backspace);
        for (const c of "u+l+ c")
            keyClick(c);
        verify(!error.visible);
        tryCompare(results, "count", 1);
        keyClick(Qt.Key_Return);
        compare(text.selectedText, "gull c");

        // Escape in the panel shows the binder again.
        keyClick(Qt.Key_F, Qt.ControlModifier | Qt.ShiftModifier);
        keyClick(Qt.Key_Escape);
        verify(!view.searchOpen);
    }

    function test_replaceShowsEveryMatchFirst() {
        const view = makeView();
        const model = view.session.binder;
        const first = model.data(model.index(1, 0), BinderModel.IdRole);
        view.session.currentId = first;
        const text = findChild(view, "editor") as TextEdit;
        text.text = "A gull, then another gull.";
        waitForRendering(view);

        view.forceActiveFocus();
        keyClick(Qt.Key_H, Qt.ControlModifier);
        verify(view.searchOpen);
        for (const c of "gull")
            keyClick(c);
        keyClick(Qt.Key_Tab);
        for (const c of "tern")
            keyClick(c);
        const preview = findChild(view, "replacePreview") as ListView;
        const button = findChild(view, "replaceButton") as Button;
        tryCompare(button, "text", "Replace 2 in 1 scene");
        compare(preview.count, 1);
        // Nothing changes before the click.
        compare(view.session.snapshots(first).length, 0);
        compare(text.text, "A gull, then another gull.");

        mouseClick(button);
        compare(text.text, "A tern, then another tern.");
        compare(view.session.snapshots(first).length, 1);
        const done = findChild(view, "replaceDone") as Text;
        verify(done.text.startsWith("Replaced 2 in 1 scene."), done.text);
        tryCompare(preview, "count", 0);
        verify(!button.enabled);
    }

    function test_savedSearchOpensItsResults() {
        const view = makeView();
        const model = view.session.binder;
        const first = model.data(model.index(1, 0), BinderModel.IdRole);
        view.session.currentId = first;
        const text = findChild(view, "editor") as TextEdit;
        text.text = "A heron waited.";

        view.forceActiveFocus();
        keyClick(Qt.Key_F, Qt.ControlModifier | Qt.ShiftModifier);
        for (const c of "heron")
            keyClick(c);
        const save = findChild(view, "saveSearch") as T.Button;
        verify(save.enabled);
        mouseClick(save);
        keyClick(Qt.Key_Escape);
        verify(!view.searchOpen);

        let row = -1;
        for (let i = 0; i < model.rowCount(); ++i) {
            if (model.data(model.index(i, 0), BinderModel.TitleRole) === "heron")
                row = i;
        }
        verify(row >= 0);
        view.session.currentId = model.data(model.index(row, 0), BinderModel.IdRole);
        verify(view.searchOpen);
        const results = findChild(view, "searchResults") as ListView;
        tryCompare(results, "count", 1);

        // The results follow a change of the text.
        view.session.currentId = first;
        text.text = "No bird here.";
        tryCompare(results, "count", 0, 3000);
    }

    function test_paletteRunsCommandsAndOpensItems() {
        const view = makeView();
        const model = view.session.binder;
        const first = model.data(model.index(1, 0), BinderModel.IdRole);
        view.session.currentId = first;
        waitForRendering(view);

        view.forceActiveFocus();
        keyClick(Qt.Key_P, Qt.ControlModifier | Qt.ShiftModifier);
        const field = findChild(view.T.Overlay.overlay, "paletteField") as TextField;
        const results = findChild(view.T.Overlay.overlay, "paletteResults") as ListView;
        verify(field && results);
        tryVerify(() => field.activeFocus);
        // With no text, the list shows every command and every item.
        verify(results.count >= view.commands.length - 1);
        verify(results.model[0].name !== "Find a command or an item");

        for (const c of "snap")
            keyClick(c);
        compare(results.model[0].name, "Take a snapshot");
        keyClick(Qt.Key_Return);
        compare(view.session.snapshots(first).length, 1);

        // An item opens by its title. The letters in order also match.
        view.session.currentId = "";
        keyClick(Qt.Key_P, Qt.ControlModifier | Qt.ShiftModifier);
        tryVerify(() => field.activeFocus);
        for (const c of "opng")
            keyClick(c);
        verify(results.count > 0);
        compare(results.model[0].id, first);
        keyClick(Qt.Key_Return);
        compare(view.session.currentId, first);

        // No match shows an empty list, and Escape closes the palette.
        keyClick(Qt.Key_P, Qt.ControlModifier | Qt.ShiftModifier);
        tryVerify(() => field.activeFocus);
        for (const c of "zzqx")
            keyClick(c);
        compare(results.count, 0);
        keyClick(Qt.Key_Return);
        keyClick(Qt.Key_Escape);
        tryVerify(() => !field.visible);
    }

    function test_everyCommandHasAKey() {
        const view = makeView();
        let seen = {};
        for (const command of view.commands) {
            verify(command.sequence !== "", command.id);
            verify(!seen[command.sequence], command.sequence);
            seen[command.sequence] = true;
        }
        const warnings = findChild(view, "keyWarnings") as Text;
        verify(!warnings.visible);

        // A key that was new in T50 runs its command.
        const before = view.session.items().length;
        view.forceActiveFocus();
        keyClick(Qt.Key_N, Qt.ControlModifier | Qt.ShiftModifier);
        compare(view.session.items().length, before + 1);
        compare(view.session.fields(view.session.currentId).kind, "chapter");
    }

    function test_layoutComesBackWhenTheProjectOpens() {
        let view = makeView();
        const folder = view.folder;
        const model = view.session.binder;
        const first = model.data(model.index(1, 0), BinderModel.IdRole);
        view.session.currentId = first;
        view.inspectorOpen = true;
        waitForRendering(view);
        // Drag the handle between the binder and the page 60 pixels right.
        const binderPane = findChild(view, "binderPane") as Item;
        const before = binderPane.width;
        mouseDrag(view, binderPane.width + 3, view.height / 2, 60, 0);
        tryVerify(() => Math.abs(binderPane.width - before - 60) <= 2);
        view.forceActiveFocus();
        keyClick(Qt.Key_B, Qt.ControlModifier | Qt.ShiftModifier);
        verify(!view.binderOpen);
        view.destroy();
        wait(0);

        view = createTemporaryObject(viewComponent, testCase, { folder: folder }) as ProjectView;
        compare(view.session.currentId, first);
        verify(!view.binderOpen);
        verify(view.inspectorOpen);
        view.binderOpen = true;
        waitForRendering(view);
        const pane = findChild(view, "binderPane") as Item;
        verify(Math.abs(pane.width - before - 60) <= 2, pane.width);
    }

    function test_inspectorSavesAField() {
        const view = makeView();
        const model = view.session.binder;
        const id = model.data(model.index(1, 0), BinderModel.IdRole);
        view.session.currentId = id;
        view.inspectorOpen = true;
        waitForRendering(view);
        const inspector = findChild(view, "inspector");
        verify(inspector);
        compare(view.session.fields(id).status, "draft");
        inspector.save("status", "final");
        compare(view.session.fields(id).status, "final");
        compare(inspector.fields.status, "final");
        // The same value again saves nothing.
        verify(!inspector.save("status", "final"));
    }

    function test_aChapterShowsAllItsScenesReadOnly() {
        const temp = StandardPaths.writableLocation(StandardPaths.TempLocation).toString().replace("file://", "");
        workspace.folder = temp + "/codicier-qml-" + Date.now() + "-" + Math.floor(Math.random() * 1e6);
        compare(workspace.create("Harbour", "novella", ""), "");
        const view = createTemporaryObject(viewComponent, testCase, { folder: workspace.folder + "/harbour" }) as ProjectView;
        const model = view.session.binder;
        const chapter = model.data(model.index(1, 0), BinderModel.IdRole);
        view.session.currentId = model.data(model.index(2, 0), BinderModel.IdRole);
        view.session.add("scene");
        view.session.currentId = chapter;
        waitForRendering(view);
        const composite = findChild(view, "composite") as Composite;
        verify(composite);
        tryCompare(composite, "count", 2);
        verify(!composite.readOnly);
    }

    function test_editsAtTheSceneBorderSaveToEachFile() {
        const temp = StandardPaths.writableLocation(StandardPaths.TempLocation).toString().replace("file://", "");
        workspace.folder = temp + "/codicier-qml-" + Date.now() + "-" + Math.floor(Math.random() * 1e6);
        compare(workspace.create("Harbour", "novella", ""), "");
        const view = createTemporaryObject(viewComponent, testCase, { folder: workspace.folder + "/harbour" }) as ProjectView;
        const model = view.session.binder;
        const chapter = model.data(model.index(1, 0), BinderModel.IdRole);
        const first = model.data(model.index(2, 0), BinderModel.IdRole);
        view.session.currentId = first;
        const second = view.session.add("scene");
        view.session.currentId = chapter;
        waitForRendering(view);
        const composite = findChild(view, "composite") as Composite;
        tryCompare(composite, "count", 2);

        // Type at the end of the first scene, cross the border with Down,
        // and type at the start of the second scene.
        verify(composite.focusBlock(0, false));
        for (const c of "End of one.")
            keyClick(c);
        keyClick(Qt.Key_Down);
        for (const c of "Start of two.")
            keyClick(c);
        // Up on the first line goes back to the end of the first scene.
        keyClick(Qt.Key_Home);
        keyClick(Qt.Key_Up);
        keyClick(Qt.Key_Space);
        keyClick(Qt.Key_A);
        wait(1300);

        // Each file holds only its own text.
        view.session.currentId = first;
        const editor = findChild(view, "editor") as TextEdit;
        tryCompare(editor, "text", "End of one. a");
        view.session.currentId = second;
        tryCompare(editor, "text", "Start of two.");
        compare(view.session.chapterWords, 7);
    }

    function test_leftOutSceneLeavesTheBookTotal() {
        const view = makeView();
        const model = view.session.binder;
        const id = model.data(model.index(1, 0), BinderModel.IdRole);
        view.session.currentId = id;
        const text = findChild(view, "editor") as TextEdit;
        text.forceActiveFocus();
        for (const c of "Four words in here")
            keyClick(c);
        tryCompare(view.session, "manuscriptWords", 4);
        view.inspectorOpen = true;
        waitForRendering(view);
        verify(findChild(view, "inspector").save("include", false));
        compare(view.session.manuscriptWords, 0);
        // The binder edit saved the typing first, so the scene keeps its count.
        compare(model.data(model.index(model.rowOf(id), 0), BinderModel.WordsRole), 4);
        compare(model.data(model.index(model.rowOf(id), 0), BinderModel.IncludedRole), false);
    }

    function test_themeChangeIsNotAnEdit() {
        const view = makeView();
        const model = view.session.binder;
        view.session.currentId = model.data(model.index(1, 0), BinderModel.IdRole);
        compare(view.session.document.file, "manuscript/opening.md");
        Settings.theme = "dark";
        Settings.theme = "light";
        compare(view.session.document.dirty, false);
    }

    Component {
        id: settingsComponent

        SettingsDialog {}
    }

    // Scrolls the flickable around item so that a click reaches it.
    function reveal(item: Item) {
        let flick = item.parent;
        while (flick && (flick as Flickable) === null)
            flick = flick.parent;
        const flickable = flick as Flickable;
        const top = item.mapToItem(flickable.contentItem, 0, 0).y;
        flickable.contentY = Math.max(0, Math.min(flickable.contentHeight - flickable.height, top - Theme.space.l));
        waitForRendering(flickable);
    }

    function test_settingsApplyLive() {
        const dialog = createTemporaryObject(settingsComponent, testCase) as SettingsDialog;
        dialog.open();
        tryVerify(() => dialog.opened);
        const size = Settings.readingSize;
        const dark = findChild(dialog.contentItem, "theme-dark") as Item;
        mouseClick(dark);
        compare(Theme.name, "dark");
        const stepper = findChild(dialog.contentItem, "sizeStepper") as Item;
        mouseClick(stepper, stepper.width - 10, stepper.height / 2);
        compare(Settings.readingSize, size + 1);
        compare(Theme.type.readingSize, size + 1);
        const measure = Settings.measure;
        const width = findChild(dialog.contentItem, "measureStepper") as Item;
        mouseClick(width, 10, width.height / 2);
        compare(Theme.type.measure, measure - 2);
        mouseClick(findChild(dialog.contentItem, "motionOff") as Item);
        verify(Theme.motion.reduced);
        compare(Theme.motion.fast, 0);
        const autosave = findChild(dialog.contentItem, "autosave-2000") as Item;
        reveal(autosave);
        mouseClick(autosave);
        compare(Settings.autosaveDelay, 2000);

        // The font picker lists the font of the app first and sets a font.
        const fontButton = findChild(dialog.contentItem, "readingFontButton") as Item;
        reveal(fontButton);
        mouseClick(fontButton);
        const list = findChild(dialog.T.Overlay.overlay, "fontList") as ListView;
        tryVerify(() => list && list.visible && list.count > 0);
        compare(list.model[0], "");
        if (list.count > 1) {
            const family = list.model[1];
            keyClick(Qt.Key_Down);
            keyClick(Qt.Key_Return);
            compare(Settings.readingFont, family);
            compare(Theme.type.reading, family);
        }

        Settings.theme = "system";
        Settings.readingSize = size;
        Settings.measure = measure;
        Settings.reducedMotion = false;
        Settings.autosaveDelay = 1000;
        Settings.readingFont = "";
        compare(Theme.type.reading, Theme.readingFont.font.family);
    }
}
