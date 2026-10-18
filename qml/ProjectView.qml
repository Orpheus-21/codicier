pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// The writing view of one project: the binder on the left, the page on the
// right, and the status bar. The layout follows the T17 mock. T29 fills the
// binder and T30 the editor.
Item {
    id: view

    // The project folder to open.
    property string folder: ""
    readonly property ProjectSession session: ProjectSession {
        folder: view.folder
    }
    // The name of the workspace, for the way back.
    property string workspaceName: ""

    // The author asked to go back to the workspace home.
    signal closeRequested

    Shortcut {
        sequence: "Ctrl+W"
        onActivated: view.closeRequested()
    }

    // A project that did not load: the error and the way back.
    Column {
        visible: view.session.error !== ""
        anchors.centerIn: parent
        width: Math.min(560, parent.width - 2 * Theme.space.xl)
        spacing: Theme.space.l

        Text {
            text: "This project did not open"
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.title
            color: Theme.color.ink
        }

        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: view.session.error
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.accent
        }

        Button {
            text: "Back to " + view.workspaceName
            onClicked: view.closeRequested()
        }
    }

    Splitter {
        visible: view.session.error === ""
        anchors.top: parent.top
        anchors.bottom: statusBar.top
        width: parent.width

        // The binder.
        Pane {
            T.SplitView.preferredWidth: 320
            T.SplitView.minimumWidth: 220
            padding: Theme.space.m

            Column {
                width: parent.width

                Button {
                    id: projectButton

                    text: view.session.title
                    iconName: "chevron-down"
                    iconAfterText: true
                    font.family: Theme.type.reading
                    font.pixelSize: Theme.type.large + 3
                    font.weight: Font.Normal
                    onClicked: projectMenu.popup(projectButton, 0, projectButton.height)
                }

                Menu {
                    id: projectMenu

                    T.Action {
                        text: "Back to " + view.workspaceName
                        onTriggered: view.closeRequested()
                    }
                }
            }
        }

        // The editor area: a quiet path line, then the page.
        Item {
            T.SplitView.fillWidth: true

            Item {
                id: editorBar

                width: parent.width
                height: 48

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.space.l
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space.xs

                    IconButton {
                        iconName: "maximize-2"
                        label: "Focus mode"
                        enabled: false
                    }

                    IconButton {
                        iconName: "panel-right"
                        label: "Inspector"
                        enabled: false
                    }
                }
            }
        }
    }

    // The status bar. T34 adds the word counts.
    Item {
        id: statusBar

        anchors.bottom: parent.bottom
        width: parent.width
        height: 36

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.color.rule
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.xl
            anchors.verticalCenter: parent.verticalCenter
            visible: view.session.readOnly
            text: "Read only: this project uses a newer format."
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            color: Theme.color.inkMuted
        }
    }
}
