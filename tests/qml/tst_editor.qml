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
