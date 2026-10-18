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

    // Saves the open text, then asks to go back to the workspace.
    function close() {
        view.session.document.save();
        view.closeRequested();
    }

    Shortcut {
        sequence: "Ctrl+W"
        onActivated: view.close()
    }

    Shortcut {
        sequence: "Ctrl+N"
        onActivated: binder.addItem("scene")
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
            onClicked: view.close()
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

            IconButton {
                id: addButton

                anchors.right: parent.right
                anchors.verticalCenter: projectButton.verticalCenter
                iconName: "plus"
                label: "New item"
                onClicked: addMenu.popup(addButton, 0, addButton.height)
            }

            Menu {
                id: addMenu

                T.Action { text: "New scene"; onTriggered: binder.addItem("scene") }
                T.Action { text: "New chapter"; onTriggered: binder.addItem("chapter") }
                T.Action { text: "New part"; onTriggered: binder.addItem("part") }
                T.Action { text: "New note"; onTriggered: binder.addItem("note") }
                T.Action { text: "New page"; onTriggered: binder.addItem("page") }
            }

            Menu {
                id: projectMenu

                T.Action {
                    text: "Back to " + view.workspaceName
                    onTriggered: view.close()
                }
            }

            Binder {
                id: binder

                // Enter on a text item moves the focus to the page.
                onActivated: editor.focusText()
                anchors.top: projectButton.bottom
                anchors.topMargin: Theme.space.s
                anchors.bottom: parent.bottom
                width: parent.width
                session: view.session
            }
        }

        // The editor area: a quiet path line, then the page.
        Item {
            T.SplitView.fillWidth: true

            Editor {
                id: editor

                anchors.top: editorBar.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                document: view.session.document
            }

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

    // The status bar: the words of the scene, the chapter, and the book.
    Item {
        id: statusBar

        function number(n: int): string {
            return Number(n).toLocaleString(Qt.locale("en_GB"), "f", 0);
        }

        anchors.bottom: parent.bottom
        width: parent.width
        height: 36

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.color.rule
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.xl
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space.l

            component Count: Text {
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.small
                color: Theme.color.inkMuted
            }

            Count {
                visible: view.session.document.file !== ""
                text: "Scene " + statusBar.number(view.session.sceneWords) + " words"
            }

            Count {
                visible: view.session.chapterWords >= 0
                text: "Chapter " + statusBar.number(view.session.chapterWords)
            }

            Count {
                visible: view.session.error === ""
                text: "Book " + statusBar.number(view.session.manuscriptWords)
                      + (view.session.goalWords > 0 ? " of " + statusBar.number(view.session.goalWords) : "")
            }

            Count {
                visible: view.session.readOnly
                text: "Read only: this project uses a newer format."
            }
        }
    }
}
