pragma ComponentBehavior: Bound

import QtCore
import QtQuick
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
        Theme.choice = "system";
    }

    function test_themeColoursReachTheDocument() {
        const view = makeView();
        const document = view.session.document;
        Theme.choice = "dark";
        verify(Qt.colorEqual(document.markColor, Theme.palettes.dark.mark));
        verify(Qt.colorEqual(document.accentColor, Theme.palettes.dark.accent));
        Theme.choice = "sepia";
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
        Theme.choice = "dark";
        Theme.choice = "light";
        compare(view.session.document.dirty, false);
    }
}
