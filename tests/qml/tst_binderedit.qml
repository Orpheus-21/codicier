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
        // Let the binder put back its current row after the model reset.
        wait(0);
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

    // The titles of the children of the chapter, in binder order.
    function scenes(binder: Binder): list<string> {
        const model = binder.session.binder;
        const out = [];
        for (let row = 0; row < model.rowCount(); ++row) {
            if (model.data(model.index(row, 0), BinderModel.DepthRole) === 2)
                out.push(model.data(model.index(row, 0), BinderModel.TitleRole));
        }
        return out;
    }

    function addScene(binder: Binder, after: string, title: string) {
        binder.session.currentId = idOf(binder, after);
        const id = binder.session.add("scene");
        binder.renamingId = "";
        verify(binder.session.rename(id, title));
        wait(0);
    }

    function drag(binder: Binder, from: string, to: string, part: real) {
        const source = binder.itemAtIndex(binder.session.binder.rowOf(idOf(binder, from)));
        const target = binder.itemAtIndex(binder.session.binder.rowOf(idOf(binder, to)));
        mousePress(source, 60, source.height / 2);
        for (let step = 1; step <= 5; ++step)
            mouseMove(source, 60, source.height / 2 + step * 6);
        mouseMove(target, 60, target.height * part);
        mouseMove(target, 60, target.height * part + 1);
        mouseRelease(target, 60, target.height * part + 1);
    }

    function test_dragAboveAnotherScene() {
        const binder = makeBinder();
        addScene(binder, "Scene 1", "Scene 2");
        waitForRendering(binder);
        compare(scenes(binder), ["Scene 1", "Scene 2"]);
        drag(binder, "Scene 2", "Scene 1", 0.1);
        tryCompare(binder, "dragId", "");
        compare(scenes(binder), ["Scene 2", "Scene 1"]);
    }

    function test_dragIntoTheMiddleOfAChapter() {
        const binder = makeBinder();
        binder.session.currentId = idOf(binder, "Part One");
        const chapter = binder.session.add("chapter");
        binder.renamingId = "";
        verify(binder.session.rename(chapter, "Chapter 2"));
        waitForRendering(binder);
        drag(binder, "Scene 1", "Chapter 2", 0.5);
        tryCompare(binder, "dragId", "");
        const model = binder.session.binder;
        const row = model.rowOf(idOf(binder, "Scene 1"));
        compare(model.data(model.index(row - 1, 0), BinderModel.TitleRole), "Chapter 2");
    }

    function test_altUpMovesTheItem() {
        const binder = makeBinder();
        addScene(binder, "Scene 1", "Scene 2");
        const id = idOf(binder, "Scene 2");
        binder.forceActiveFocus();
        binder.currentIndex = binder.session.binder.rowOf(id);
        keyClick(Qt.Key_Up, Qt.AltModifier);
        compare(scenes(binder), ["Scene 2", "Scene 1"]);
        tryCompare(binder, "currentIndex", binder.session.binder.rowOf(id));
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
