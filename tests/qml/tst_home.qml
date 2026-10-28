pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Templates as T
import QtTest
import Codicier

TestCase {
    id: testCase

    name: "Home"
    when: windowShown
    visible: true
    width: 1000
    height: 700

    Component {
        id: homeComponent

        WorkspaceHome {
            anchors.fill: parent
        }
    }

    // A workspace with one project, shown on the home screen.
    function makeHome(): WorkspaceHome {
        const temp = StandardPaths.writableLocation(StandardPaths.TempLocation).toString().replace("file://", "");
        const folder = temp + "/codicier-home-" + Date.now() + "-" + Math.floor(Math.random() * 1e6);
        const maker = createTemporaryQmlObject("import Codicier; ProjectList {}", testCase);
        maker.folder = folder;
        compare(maker.create("Harbour", "novel", ""), "");
        const home = createTemporaryObject(homeComponent, testCase, { folder: folder }) as WorkspaceHome;
        compare(home.projects.count, 1);
        return home;
    }

    function test_rowMenuRenamesCopiesAndArchives() {
        const home = makeHome();
        waitForRendering(home);
        mouseClick(findChild(home, "moreProject") as Item);
        const menu = findChild(home, "rowMenu") as T.Menu;
        tryVerify(() => menu.opened);
        compare(menu.count, 4);
        // Make a copy.
        menu.actionAt(1).trigger();
        compare(home.projects.count, 2);
        compare(home.projectError, "");
        // Rename: the dialog sends the new title.
        menu.actionAt(0).trigger();
        const dialog = findChild(home, "renameDialog") as T.Dialog;
        tryVerify(() => dialog.opened);
        (findChild(dialog, "lineField") as TextField).text = "Lighthouse";
        keyClick(Qt.Key_Return);
        tryVerify(() => !dialog.visible);
        const titles = [home.projects.get(0).title, home.projects.get(1).title];
        verify(titles.includes("Lighthouse"), titles.join());
        // Archive takes a project off the list. The menu works on the row
        // that opened it.
        menu.close();
        mouseClick(findChild(home, "moreProject") as Item);
        tryVerify(() => menu.opened);
        menu.actionAt(3).trigger();
        compare(home.projectError, "");
        compare(home.projects.count, 1);
    }
}
