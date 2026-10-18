pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtTest
import Codicier

TestCase {
    id: testCase

    name: "BinderEdit"
    when: windowShown
    visible: true
    width: 400
    height: 700

    // A new novel in a fresh temp folder for each test, so no test changes
    // the fixtures in the repo.
    ProjectList {
        id: workspace
    }

    Component {
        id: binderComponent

        Binder {
            width: 360
            height: 680
            session: ProjectSession {}
        }
    }

    function makeBinder(): Binder {
        const temp = StandardPaths.writableLocation(StandardPaths.TempLocation).toString().replace("file://", "");
        workspace.folder = temp + "/codicier-qml-" + Date.now() + "-" + Math.floor(Math.random() * 1e6);
        compare(workspace.create("Harbour", "novel", ""), "");
        const binder = createTemporaryObject(binderComponent, testCase) as Binder;
        binder.session.folder = workspace.folder + "/harbour";
        compare(binder.session.error, "");
        waitForRendering(binder);
        return binder;
    }

    function idOf(binder: Binder, title: string): string {
        const model = binder.session.binder;
        for (let row = 0; row < model.rowCount(); ++row) {
            if (model.data(model.index(row, 0), BinderModel.TitleRole) === title)
                return model.data(model.index(row, 0), BinderModel.IdRole);
        }
        return "";
    }

    function test_addStartsARenameAndEnterSavesIt() {
        const binder = makeBinder();
        binder.session.currentId = idOf(binder, "Scene 1");
        binder.addItem("scene");
        const id = binder.session.currentId;
        compare(binder.renamingId, id);
        waitForRendering(binder);
        // The title in the field is selected, so typing replaces it.
        for (const c of "The Lock")
            keyClick(c);
        keyClick(Qt.Key_Return);
        compare(binder.renamingId, "");
        compare(idOf(binder, "The Lock"), id);
    }

    function test_f2RenamesAndEscapeCancels() {
        const binder = makeBinder();
        const id = idOf(binder, "Chapter 1");
        binder.forceActiveFocus();
        binder.currentIndex = binder.session.binder.rowOf(id);
        keyClick(Qt.Key_F2);
        compare(binder.renamingId, id);
        keyClick(Qt.Key_Escape);
        compare(binder.renamingId, "");
        compare(idOf(binder, "Chapter 1"), id);
    }

    function test_deleteMovesToTheTrash() {
        const binder = makeBinder();
        const id = idOf(binder, "Scene 1");
        binder.forceActiveFocus();
        binder.currentIndex = binder.session.binder.rowOf(id);
        keyClick(Qt.Key_Delete);
        verify(binder.session.binder.isInTrash(id));
    }
}
