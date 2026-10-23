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
        // A scene question shows as a column once a row has it.
        const outliner = findChild(view, "outlinerView") as Outliner;
        verify(!outliner.columns.some(column => column.key === "goal"));
        verify(view.session.setField(first, "goal", "Light the lamp"));
        tryVerify(() => outliner.columns.some(column => column.key === "goal"));
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
        // Ctrl+Shift+H puts a highlight around the word.
        text.text = "dusk";
        text.cursorPosition = 2;
        keyClick(Qt.Key_H, Qt.ControlModifier | Qt.ShiftModifier);
        compare(text.text, "==dusk==");
        keyClick(Qt.Key_H, Qt.ControlModifier | Qt.ShiftModifier);
        compare(text.text, "dusk");
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

    function test_marksHideOutsideTheCursorParagraph() {
        const view = makeView();
        verify(view.session.hideMarks);
        const text = findChild(view, "editor") as TextEdit;
        text.text = "a **b**\u2029c **d**";
        text.forceActiveFocus();
        text.cursorPosition = 1;
        waitForRendering(text);
        // The second paragraph hides its marks, so it is narrower than the
        // first, which shows them.
        const first = text.positionToRectangle(7).x - text.positionToRectangle(0).x;
        const second = text.positionToRectangle(15).x - text.positionToRectangle(8).x;
        verify(second < first - 5, first + " " + second);
        // The cursor moves, and the marks move with it.
        text.cursorPosition = 9;
        waitForRendering(text);
        verify(text.positionToRectangle(7).x - text.positionToRectangle(0).x < first - 5);
        Settings.hideMarks = false;
        waitForRendering(text);
        fuzzyCompare(text.positionToRectangle(15).x - text.positionToRectangle(8).x, first, 3);
        Settings.hideMarks = true;
    }

    function test_altArrowsWalkThroughTheOpenedTexts() {
        const temp = StandardPaths.writableLocation(StandardPaths.TempLocation).toString().replace("file://", "");
        workspace.folder = temp + "/codicier-qml-" + Date.now() + "-" + Math.floor(Math.random() * 1e6);
        compare(workspace.create("Lamp", "novel", ""), "");
        const view = createTemporaryObject(viewComponent, testCase, { folder: workspace.folder + "/lamp" }) as ProjectView;
        const text = findChild(view, "editor") as TextEdit;
        const first = view.session.currentId;
        // The page finishes its load before the author moves the cursor.
        wait(0);
        text.text = "The lamp burned all night.";
        text.cursorPosition = 9;
        const second = view.session.add("scene");
        view.session.currentId = second;
        wait(0);
        compare(view.trail.length, 2);
        keyClick(Qt.Key_Left, Qt.AltModifier);
        compare(view.session.currentId, first);
        tryCompare(text, "cursorPosition", 9);
        keyClick(Qt.Key_Right, Qt.AltModifier);
        compare(view.session.currentId, second);
        // At the end of the trail, Alt+Right does nothing.
        keyClick(Qt.Key_Right, Qt.AltModifier);
        compare(view.session.currentId, second);
        // A second walk back still finds the place.
        keyClick(Qt.Key_Left, Qt.AltModifier);
        tryCompare(text, "cursorPosition", 9);
    }

    function test_secondPageEditsAnotherText() {
        const temp = StandardPaths.writableLocation(StandardPaths.TempLocation).toString().replace("file://", "");
        workspace.folder = temp + "/codicier-qml-" + Date.now() + "-" + Math.floor(Math.random() * 1e6);
        compare(workspace.create("Lamp", "novel", ""), "");
        const view = createTemporaryObject(viewComponent, testCase, { folder: workspace.folder + "/lamp" }) as ProjectView;
        const main = findChild(view, "editor") as TextEdit;
        const first = view.session.currentId;
        wait(0);
        main.text = "Dusk.";
        const second = view.session.add("scene");
        view.session.currentId = second;
        // The open text cannot go beside itself.
        view.openBeside(second);
        compare(view.sideId, "");
        // Ctrl+\\ opens the text before this one beside it.
        view.forceActiveFocus();
        keyClick(Qt.Key_Backslash, Qt.ControlModifier);
        compare(view.sideId, first);
        const side = findChild(view, "sideText") as TextEdit;
        verify(side.visible);
        compare(side.text, "Dusk.");
        main.text = "Dawn.";
        side.text = "Dusk fell.";
        tryVerify(() => !view.session.document.dirty && !side.document.dirty, 5000);
        // Opening the side text on the main page closes the second page.
        view.session.currentId = first;
        compare(view.sideId, "");
        // Each page saved its own file.
        compare(main.text, "Dusk fell.");
        view.session.currentId = second;
        compare(main.text, "Dawn.");
    }

    function test_zoomKeysChangeTheTextSize() {
        const view = makeView();
        view.forceActiveFocus();
        keyClick(Qt.Key_0, Qt.ControlModifier);
        compare(Settings.readingSize, 19);
        keyClick(Qt.Key_Equal, Qt.ControlModifier);
        keyClick(Qt.Key_Equal, Qt.ControlModifier);
        compare(Settings.readingSize, 21);
        compare(Theme.type.readingSize, 21);
        keyClick(Qt.Key_Minus, Qt.ControlModifier);
        compare(Settings.readingSize, 20);
        keyClick(Qt.Key_0, Qt.ControlModifier);
        compare(Settings.readingSize, 19);
    }

    function test_languageDialogSetsTheDictionary() {
        const view = makeView();
        const dialog = findChild(view, "languageDialog") as LanguageDialog;
        dialog.open();
        tryVerify(() => dialog.opened);
        compare(dialog.names, ["en_TEST", "fr_TEST"]);
        mouseClick(findChild(dialog.contentItem, "language-fr_TEST") as Item);
        compare(view.session.dictionary, "fr_TEST");
        tryVerify(() => !dialog.visible);
    }

    function test_rightClickOffersSynonyms() {
        const view = makeView();
        const text = findChild(view, "editor") as TextEdit;
        text.text = "the lamp";
        const editorArea = findChild(view, "editorArea") as Editor;
        // The test dictionary does not know "lamp", so the check is off here.
        Settings.spellCheck = false;
        editorArea.suggestAt(6, 10, 10);
        Settings.spellCheck = true;
        const menu = findChild(editorArea, "synonymMenu") as Menu;
        tryVerify(() => menu.opened);
        compare(editorArea.wordSynonyms, ["light", "lantern", "beacon"]);
        menu.actionAt(1).trigger();
        compare(text.text, "the lantern");
    }

    function test_binderActsOnManySelectedItems() {
        const temp = StandardPaths.writableLocation(StandardPaths.TempLocation).toString().replace("file://", "");
        workspace.folder = temp + "/codicier-qml-" + Date.now() + "-" + Math.floor(Math.random() * 1e6);
        compare(workspace.create("Lamp", "novel", ""), "");
        const view = createTemporaryObject(viewComponent, testCase, { folder: workspace.folder + "/lamp" }) as ProjectView;
        const binder = findChild(view, "binder") as Binder;
        const first = view.session.currentId;
        const second = view.session.add("scene");
        view.session.currentId = second;
        const third = view.session.add("scene");
        view.session.currentId = first;
        waitForRendering(binder);
        const rowItem = id => binder.itemAtIndex(view.session.binder.rowOf(id));
        // Ctrl adds one item to the open one.
        mouseClick(rowItem(third), 40, 10, Qt.LeftButton, Qt.ControlModifier);
        compare(binder.selectedIds, [first, third]);
        compare(view.session.currentId, first);
        // Shift selects the range from the open item.
        mouseClick(rowItem(third), 40, 10, Qt.LeftButton, Qt.ShiftModifier);
        compare(binder.selectedIds.length, 3);
        binder.setStatus(second, "final");
        binder.addTag(second, "storm");
        for (const id of [first, second, third]) {
            compare(view.session.fields(id).status, "final");
            compare(view.session.fields(id).tags, ["storm"]);
        }
        // A drag of one selected item moves all of them, in binder order.
        const rows = () => [first, second, third].map(id => view.session.binder.rowOf(id));
        const before = rows();
        binder.selectedIds = [second, third];
        binder.dragId = third;
        binder.dropRow = before[0];
        binder.dropPosition = "before";
        binder.drop();
        const after = rows();
        verify(after[1] < after[2] && after[2] < after[0], JSON.stringify(after));
        // A colour label from the menu reaches the binder, the cards, and the
        // outliner.
        binder.selectedIds = [];
        binder.menuId = first;
        binder.setLabel(first, "green");
        compare(view.session.fields(first).label, "green");
        waitForRendering(binder);
        verify(Qt.colorEqual(rowItem(first).labelColor, Theme.labelColor("green")));
        // Numbers show in the binder rows when the project turns them on.
        verify(view.session.setNumbered(true));
        waitForRendering(binder);
        // The drag above put the first scene in third place.
        compare(rowItem(first).number, "1.3");
        compare(rowItem(second).number, "1.1");
        verify(view.session.setNumbered(false));
        compare(binder.numbers, {});
        binder.selectedIds = [first, second, third];
        binder.trash(first);
        for (const id of [first, second, third])
            verify(view.session.binder.isInTrash(id));
        compare(binder.selectedIds, []);
    }

    function test_binderDragShowsWhereTheItemLands() {
        const temp = StandardPaths.writableLocation(StandardPaths.TempLocation).toString().replace("file://", "");
        workspace.folder = temp + "/codicier-qml-" + Date.now() + "-" + Math.floor(Math.random() * 1e6);
        compare(workspace.create("Lamp", "novel", ""), "");
        const view = createTemporaryObject(viewComponent, testCase, { folder: workspace.folder + "/lamp" }) as ProjectView;
        const binder = findChild(view, "binder") as Binder;
        const scene = view.session.currentId;
        const second = view.session.add("scene");
        const chapter = view.session.fields(scene).parent;
        waitForRendering(binder);
        const marker = findChild(binder, "dropMarker") as Rectangle;
        const rowOf = id => binder.itemAtIndex(view.session.binder.rowOf(id));
        binder.dragId = second;
        // The top of a scene row: a line before it.
        const top = rowOf(scene);
        binder.dragTo(top.y - binder.contentY + 2);
        compare(binder.dropPosition, "before");
        verify(marker.visible);
        compare(marker.height, 2);
        // The middle of the chapter row: a frame, and the scene goes inside.
        const folder = rowOf(chapter);
        binder.dragTo(folder.y - binder.contentY + folder.height / 2);
        compare(binder.dropPosition, "inside");
        compare(marker.height, folder.height);
        verify(marker.border.width > 0);
        binder.dragId = "";
        binder.dropRow = -1;
        verify(!marker.visible);
    }

    function test_hoverOverACodexNameShowsItsCard() {
        const view = makeView();
        const id = view.session.add("character");
        verify(view.session.rename(id, "Elin"));
        const scene = view.session.items().find(item => item.kind === "scene").id;
        view.session.currentId = scene;
        const text = findChild(view, "editor") as TextEdit;
        text.text = "Then Elin climbed.";
        waitForRendering(text);
        const card = findChild(view, "entityCard") as Rectangle;
        verify(!card.visible);
        const rect = text.positionToRectangle(6);
        mouseMove(text, rect.x + 2, rect.y + rect.height / 2);
        tryVerify(() => card.visible);
        // Ctrl+click on the name opens the entry.
        mouseClick(text, rect.x + 2, rect.y + rect.height / 2, Qt.LeftButton, Qt.ControlModifier);
        tryCompare(view.session, "currentId", id);
    }

    function test_citationsCompleteAndShowTheSource() {
        const view = makeView();
        compare(view.session.saveSource({ title: "Salt", authors: "Kurlansky, Mark", year: "2002", publisher: "Walker" }),
                "kurlansky2002");
        view.session.saveSource({ title: "Cod", authors: "Kurlansky, Mark", year: "1997" });
        const scene = view.session.items().find(item => item.kind === "scene").id;
        view.session.currentId = scene;
        const text = findChild(view, "editor") as TextEdit;
        text.text = "";
        text.forceActiveFocus();
        for (const c of "Salt [@salt")
            keyClick(c);
        const list = findChild(view, "linkList") as Item;
        verify(list.visible);
        keyClick(Qt.Key_Return);
        compare(text.text, "Salt [@kurlansky2002]");
        compare(text.cursorPosition, text.text.length);
        verify(!list.visible);

        // A second key after "; @" goes before the closing bracket.
        text.cursorPosition = text.text.length - 1;
        for (const c of "; @1997")
            keyClick(c);
        verify(list.visible);
        keyClick(Qt.Key_Return);
        compare(text.text, "Salt [@kurlansky2002; @kurlansky1997]");

        // A pause on the key shows the source.
        waitForRendering(text);
        const card = findChild(view, "entityCard") as Rectangle;
        const rect = text.positionToRectangle(10);
        mouseMove(text, rect.x + 2, rect.y + rect.height / 2);
        tryVerify(() => card.visible);
        compare((findChild(card, "entitySummary") as Text).text, "Kurlansky, Mark. Walker");
    }

    function test_quoteBankInsertsAQuoteWithItsCitation() {
        const view = makeView();
        view.session.saveSource({ title: "Salt", authors: "Kurlansky, Mark", year: "2002" });
        const scene = view.session.items().find(item => item.kind === "scene").id;
        view.session.currentId = scene;
        const text = findChild(view, "editor") as TextEdit;
        text.text = "As he says, ";
        view.commands.find(command => command.id === "quotes").run();
        const panel = findChild(view, "quotesPanel") as QuotesPanel;
        verify(panel.visible);
        (findChild(panel, "quoteWords") as TextArea).text = "Salt is so common.";
        panel.source = "kurlansky2002";
        (findChild(panel, "quotePage") as TextField).text = "12";
        waitForRendering(view);
        mouseClick(findChild(panel, "addQuote") as Item);
        compare(view.session.quotes().length, 1);
        compare(view.session.quotes()[0].label, "Kurlansky 2002, Salt");

        text.cursorPosition = text.text.length;
        waitForRendering(view);
        mouseClick(findChild(panel, "insertQuote") as Item);
        compare(text.text, "As he says, “Salt is so common.” [@kurlansky2002, p. 12]");
        verify(text.activeFocus);

        // Return on the list puts it in too.
        const list = findChild(panel, "quoteList") as ListView;
        list.forceActiveFocus();
        keyClick(Qt.Key_Return);
        verify(text.text.endsWith("]“Salt is so common.” [@kurlansky2002, p. 12]"));
    }

    function test_researchPdfShowsItsPages() {
        const view = makeView();
        const id = view.session.addResearch(Qt.resolvedUrl("../fixtures/research/salt.pdf"));
        verify(id !== "");
        waitForRendering(view);
        const research = findChild(view, "researchView") as ResearchView;
        verify(research.visible);
        // A build with no PDF viewer offers the app of the system.
        if (!Pdf.available) {
            verify((findChild(research, "openExternally") as Item).visible);
            return;
        }
        const pages = findChild(research, "pdfPages") as ListView;
        compare(pages.count, 2);
        tryVerify(() => findChild(pages, "pdfPage-0") !== null);
        const paper = (findChild(pages, "pdfPage-0") as Item).children[0];
        verify(Math.abs(paper.height / paper.width - 400 / 300) < 0.01);
        tryVerify(() => (paper.children[0] as PdfPage).ready, 5000);
    }

    function test_cardsTakeTheColourOfTheirPov() {
        const temp = StandardPaths.writableLocation(StandardPaths.TempLocation).toString().replace("file://", "");
        workspace.folder = temp + "/codicier-qml-" + Date.now() + "-" + Math.floor(Math.random() * 1e6);
        compare(workspace.create("Lamp", "novel", ""), "");
        const view = createTemporaryObject(viewComponent, testCase, { folder: workspace.folder + "/lamp" }) as ProjectView;
        const first = view.session.currentId;
        const second = view.session.add("scene");
        view.session.setField(first, "pov", "Elin");
        view.session.setField(second, "pov", "Tomas");
        view.session.currentId = first;
        view.viewMode = "board";
        const board = findChild(view, "corkboardView") as Corkboard;
        tryCompare(board.cards, "length", 2);
        Settings.cardColour = "pov";
        compare(board.povs.slice().sort(), ["Elin", "Tomas"]);
        // A POV has the same colour on every board: its place in the manuscript.
        verify(Qt.colorEqual(board.cardColor(first), Theme.color.series[board.allPovs.indexOf("Elin")]));
        verify(!Qt.colorEqual(board.cardColor(first), board.cardColor(second)));
        Settings.cardColour = "label";
        verify(Qt.colorEqual(board.cardColor(second), "transparent"));
    }

    function test_nameDialogAddsACharacter() {
        const view = makeView();
        view.forceActiveFocus();
        keyClick(Qt.Key_A, Qt.ControlModifier | Qt.AltModifier);
        const dialog = findChild(view, "nameDialog") as NameDialog;
        tryVerify(() => dialog.opened);
        compare(dialog.names.length, 12);
        dialog.culture = "Japanese";
        dialog.invented = true;
        compare(dialog.names.length, 12);
        const name = dialog.names[0];
        dialog.addCharacter(name);
        tryVerify(() => !dialog.visible);
        compare(view.session.fields(view.session.currentId).title, name);
        compare(view.session.fields(view.session.currentId).kind, "character");
    }

    function test_freeBoardKeepsAndStacksCards() {
        const temp = StandardPaths.writableLocation(StandardPaths.TempLocation).toString().replace("file://", "");
        workspace.folder = temp + "/codicier-qml-" + Date.now() + "-" + Math.floor(Math.random() * 1e6);
        compare(workspace.create("Lamp", "novel", ""), "");
        const view = createTemporaryObject(viewComponent, testCase, { folder: workspace.folder + "/lamp" }) as ProjectView;
        const first = view.session.currentId;
        const second = view.session.add("scene");
        view.session.currentId = first;
        view.viewMode = "board";
        const board = findChild(view, "corkboardView") as Corkboard;
        tryCompare(board.cards, "length", 2);
        Settings.freeBoard = true;
        const free = findChild(board, "freeBoard") as Flickable;
        waitForRendering(free);
        const cards = [];
        for (const child of free.contentItem.children) {
            if (child.objectName === "freeCard")
                cards.push(child);
        }
        compare(cards.length, 2);
        // A drag moves the card, and its place stays.
        const moved = cards.find(card => card.item.id === second);
        mouseDrag(moved, 20, 20, 300, 40);
        tryVerify(() => view.session.fields(second).boardX > 250);
        // On another card, it snaps beside it as a stack.
        const other = board.placeOf(first, 0);
        board.placeCard(second, other.x + 10, other.y + 5);
        compare(view.session.fields(second).boardX, other.x + 18);
        compare(view.session.fields(second).boardY, other.y + 18);
        // The card on the board moves there too.
        let shown = null;
        for (const child of free.contentItem.children) {
            if (child.objectName === "freeCard" && child.item.id === second)
                shown = child;
        }
        tryCompare(shown, "x", other.x + 18);
        compare(board.raised[board.raised.length - 1], second);
        Settings.freeBoard = false;
    }

    function test_ideaCanvasKeepsNotesCardsAndLines() {
        const view = makeView();
        const scene = view.session.currentId;
        const id = view.session.addCanvas();
        compare(view.session.currentId, id);
        const canvas = findChild(view, "canvasView") as CanvasView;
        tryVerify(() => canvas.visible);
        const surface = findChild(canvas, "canvasSurface") as Flickable;
        waitForRendering(surface);
        // A double-click on an empty spot makes a note, and the typing goes in.
        mouseDoubleClickSequence(surface, 200, 200);
        tryCompare(canvas.nodes, "length", 1);
        for (const c of "Lamp")
            keyClick(c);
        tryCompare(canvas.nodes[0], "text", "Lamp");
        const card = canvas.addNode(500, 200, "", scene);
        canvas.link(canvas.nodes[0].id, card);
        canvas.link(card, canvas.nodes[0].id);
        compare(canvas.edges.length, 1);
        // The file holds the result after the pause.
        tryVerify(() => view.session.canvasOf(id).edges.length === 1, 3000);
        compare(view.session.canvasOf(id).nodes[1].item, scene);
        // Delete takes the chosen node and its lines.
        canvas.chosenId = card;
        canvas.forceActiveFocus();
        keyClick(Qt.Key_Delete);
        compare(canvas.nodes.length, 1);
        compare(canvas.edges.length, 0);
        tryVerify(() => view.session.canvasOf(id).nodes.length === 1, 3000);
    }

    function test_changesPanelTracksAndRejects() {
        const view = makeView();
        const text = findChild(view, "editor") as TextEdit;
        text.text = "The cat sat.";
        view.forceActiveFocus();
        keyClick(Qt.Key_R, Qt.ControlModifier | Qt.ShiftModifier);
        verify(view.changesOpen);
        const panel = findChild(view, "changesPanel") as ChangesPanel;
        mouseClick(findChild(panel, "startTracking") as Item);
        verify(view.session.tracking);
        text.forceActiveFocus();
        text.cursorPosition = text.length;
        for (const c of " Then it slept.")
            keyClick(c);
        tryVerify(() => panel.entries.length === 1);
        compare(panel.entries[0].added.trim(), "Then it slept.");
        view.session.rejectChange(0);
        compare(text.text, "The cat sat.");
        tryCompare(panel.entries, "length", 0);
        view.session.acceptAll();
        verify(!view.session.tracking);
    }

    function test_recoveryBarOffersTheCompare() {
        const view = makeView();
        const bar = findChild(view, "recoveryBar") as Rectangle;
        verify(!bar.visible);
        const snapshot = view.session.takeSnapshot(view.session.currentId, "Recovered after a crash");
        view.session.recoveredSnapshot = snapshot;
        verify(bar.visible);
        mouseClick(findChild(bar, "compareRecovered") as Item);
        compare(view.comparePath, snapshot);
        verify(!bar.visible);
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
        // Calm by default: only the page shows.
        verify(view.calm);
        Settings.calmFocus = false;
        verify(!view.calm);
        Settings.calmFocus = true;
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

    function test_projectOpensAtTheLastPlace() {
        let view = makeView();
        const folder = view.folder;
        const id = view.session.currentId;
        const text = findChild(view, "editor") as TextEdit;
        text.text = Array(80).fill("The tide came in over the harbour wall.").join("\n\n");
        waitForRendering(view);
        text.cursorPosition = 2000;
        waitForRendering(view);
        // A scroll a little past the place the cursor brings, with the
        // cursor still in view.
        const page = findChild(view, "page") as Flickable;
        const scroll = Math.round(page.contentY) + 50;
        page.contentY = scroll;
        view.destroy();
        wait(0);

        view = createTemporaryObject(viewComponent, testCase, { folder: folder }) as ProjectView;
        compare(view.session.currentId, id);
        const again = findChild(view, "editor") as TextEdit;
        tryCompare(again, "cursorPosition", 2000);
        tryCompare(findChild(view, "page") as Flickable, "contentY", scroll);
    }

    function test_argumentMapShowsClaimsWithNoEvidence() {
        const temp = StandardPaths.writableLocation(StandardPaths.TempLocation).toString().replace("file://", "");
        workspace.folder = temp + "/codicier-qml-" + Date.now() + "-" + Math.floor(Math.random() * 1e6);
        compare(workspace.create("On Salt", "essay", ""), "");
        const view = createTemporaryObject(viewComponent, testCase, { folder: workspace.folder + "/on-salt" }) as ProjectView;
        view.toggleViewMode("argument");
        waitForRendering(view);
        const summary = findChild(view, "argumentSummary") as Text;
        compare(summary.text, "2 claims, 2 with no evidence");

        // The evidence note supports the first claim in the inspector.
        const items = view.session.items();
        const claim = items.find(item => item.title === "Claim 1").id;
        view.session.currentId = items.find(item => item.title === "Evidence").id;
        view.inspectorOpen = true;
        waitForRendering(view);
        const choice = findChild(view, "supports-" + claim) as Item;
        verify(choice && choice.visible);
        mouseClick(choice);
        tryCompare(summary, "text", "2 claims, 1 with no evidence");
        verify(!(findChild(findChild(view, "claim-" + claim), "noEvidence") as Item).visible);
    }

    function test_sourcesDialogAddsChangesAndRemoves() {
        const view = makeView();
        view.commands.find(command => command.id === "sources").run();
        const overlay = view.T.Overlay.overlay;
        tryVerify(() => (findChild(overlay, "addSource") as Item)?.visible ?? false);
        mouseClick(findChild(overlay, "addSource") as Item);
        const authors = findChild(overlay, "source-authors") as TextField;
        tryVerify(() => authors.activeFocus);
        authors.text = "Kurlansky, Mark";
        (findChild(overlay, "source-title") as TextField).text = "Salt";
        (findChild(overlay, "source-year") as TextField).text = "2002";
        // The dialog grows for the form, so wait for its layout.
        waitForRendering(view);
        mouseClick(findChild(overlay, "type-article-journal") as Item);
        mouseClick(findChild(overlay, "saveSource") as Item);
        tryVerify(() => findChild(overlay, "source-kurlansky2002") !== null);
        const saved = view.session.sources()[0];
        compare(saved.label, "Kurlansky 2002, Salt");
        compare(saved.type, "article-journal");

        // A change keeps the key.
        const row = findChild(overlay, "source-kurlansky2002") as Item;
        waitForRendering(view);
        mouseClick(findChild(row, "changeSource") as Item);
        tryCompare(findChild(overlay, "source-title") as TextField, "text", "Salt");
        (findChild(overlay, "source-title") as TextField).text = "Salt: A World History";
        waitForRendering(view);
        mouseClick(findChild(overlay, "saveSource") as Item);
        tryVerify(() => view.session.sources()[0].title === "Salt: A World History");
        compare(view.session.sources().length, 1);

        view.session.removeSource("kurlansky2002");
        compare(view.session.sources().length, 0);
        keyClick(Qt.Key_Escape);
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
        // The break mark shows before the second scene only.
        const marks = [0, 1].map(i => findChild(composite.itemAtIndex(i), "sceneBreakMark") as Text);
        verify(!marks[0].visible);
        verify(marks[1].visible);
        verify(view.session.setSceneBreak(""));
        verify(!marks[1].visible);
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
        const motionOff = findChild(dialog.contentItem, "motionOff") as Item;
        reveal(motionOff);
        mouseClick(motionOff);
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
