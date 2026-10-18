import QtQuick
import QtTest
import Codicier

TestCase {
    id: testCase

    name: "ProjectView"
    when: windowShown
    visible: true
    width: 1000
    height: 700

    Component {
        id: viewComponent

        ProjectView {
            anchors.fill: parent
            workspaceName: "Writing Desk"
        }
    }

    SignalSpy {
        id: closeSpy

        signalName: "closeRequested"
    }

    function test_badFolderShowsTheError() {
        const view = createTemporaryObject(viewComponent, testCase, { folder: "/no/such/project" }) as ProjectView;
        verify(view);
        verify(view.session.error.includes("does not exist"), view.session.error);
    }

    function test_ctrlWClosesTheProject() {
        const view = createTemporaryObject(viewComponent, testCase, { folder: "/no/such/project" }) as ProjectView;
        closeSpy.target = view;
        closeSpy.clear();
        view.forceActiveFocus();
        keyClick(Qt.Key_W, Qt.ControlModifier);
        compare(closeSpy.count, 1);
    }
}
