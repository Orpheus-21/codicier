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
