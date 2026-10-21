pragma ComponentBehavior: Bound

import QtQuick
import QtTest
import Codicier

TestCase {
    id: testCase

    name: "Binder"
    when: windowShown
    visible: true
    width: 400
    height: 700

    // The fixture novel, as a local path.
    readonly property string fixture: Qt.resolvedUrl("../fixtures/novel").toString().replace("file://", "")

    Component {
        id: binderComponent

        Binder {
            width: 360
            height: 680
            session: ProjectSession {
                folder: testCase.fixture
            }
        }
    }

    SignalSpy {
        id: activatedSpy

        signalName: "activated"
    }

    function makeBinder(): Binder {
        const binder = createTemporaryObject(binderComponent, testCase) as Binder;
        verify(binder);
        compare(binder.session.error, "");
        waitForRendering(binder);
        return binder;
    }

    function test_clickOpensAScene() {
        const binder = makeBinder();
        const row = binder.session.binder.rowOf("t6v4h9r2mz");
        verify(row > 0);
        const item = binder.itemAtIndex(row);
        verify(item);
        activatedSpy.target = binder;
        activatedSpy.clear();
        mouseClick(item);
        compare(binder.session.currentId, "t6v4h9r2mz");
        compare(binder.currentIndex, row);
        // The view moves the focus to the page.
        compare(activatedSpy.count, 1);
    }

    function test_clickOnASectionOnlyToggles() {
        const binder = makeBinder();
        const trash = binder.session.binder.rowOf("section:trash");
        activatedSpy.target = binder;
        activatedSpy.clear();
        mouseClick(binder.itemAtIndex(trash));
        compare(binder.session.currentId, "");
        compare(activatedSpy.count, 0);
        tryVerify(() => binder.session.binder.rowOf("e5f6g7h8j9") > trash);
    }

    function test_keysCloseAndOpen() {
        const binder = makeBinder();
        const part = binder.session.binder.rowOf("a1b2c3d4e5");
        binder.forceActiveFocus();
        binder.currentIndex = part;
        keyClick(Qt.Key_Left);
        compare(binder.session.binder.rowOf("t6v4h9r2mz"), -1);
        tryCompare(binder, "currentIndex", part);
        keyClick(Qt.Key_Right);
        verify(binder.session.binder.rowOf("t6v4h9r2mz") > part);
        tryCompare(binder, "currentIndex", part);
        keyClick(Qt.Key_Down);
        keyClick(Qt.Key_Return);
        compare(binder.session.currentId, "f6g7h8j9k0");
    }
}
