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
        backupFolder: Settings.backupFolder
        backupsToKeep: Settings.backupsToKeep
    }
    // The name of the workspace, for the way back.
    property string workspaceName: ""

    // The author asked to go back to the workspace home.
    signal closeRequested

    // The inspector pane on the right.
    property bool inspectorOpen: false
    // The snapshot that the compare view shows, or "".
    property string comparePath: ""
    property string compareLabel: ""

    Connections {
        target: view.session

        function onCurrentIdChanged() {
            view.comparePath = "";
        }
    }

    Shortcut {
        sequence: "Ctrl+I"
        onActivated: view.inspectorOpen = !view.inspectorOpen
    }

    // Focus mode: full screen with only the page, and typewriter scroll.
    property bool focusMode: false
    // The window state before focus mode, to restore it after.
    property int windowVisibility: Window.Windowed

    function setFocusMode(on: bool) {
        if (on === focusMode)
            return;
        const window = view.Window.window;
        if (on && window) {
            windowVisibility = window.visibility;
            window.visibility = Window.FullScreen;
        } else if (window) {
            window.visibility = windowVisibility;
        }
        focusMode = on;
        editor.focusText();
    }

    // Splits the open scene at the cursor, and starts the rename of the new
    // scene.
    Shortcut {
        sequence: "Ctrl+K"
        onActivated: {
            const id = view.session.split(view.session.currentId, editor.cursorPosition);
            if (id !== "")
                binder.startRename(id);
        }
    }

    // Takes a snapshot of the open text.
    Shortcut {
        sequence: "Ctrl+Shift+S"
        onActivated: view.session.takeSnapshot(view.session.currentId, "")
    }

    Shortcut {
        sequence: "F11"
        onActivated: view.setFocusMode(!view.focusMode)
    }

    Shortcut {
        sequence: "Escape"
        enabled: view.focusMode
        onActivated: view.setFocusMode(false)
    }

    // Saves the open text, then asks to go back to the workspace.
    function close() {
        view.session.backup();
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
            visible: !view.focusMode
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

            // A chapter, a part, or a folder: all its scenes as one scroll.
            Loader {
                id: compositeLoader

                readonly property bool show: view.session.currentId !== ""
                                             && view.session.document.file === ""
                                             && view.session.textsUnder(view.session.currentId).length > 0

                active: show
                visible: show
                anchors.top: editorBar.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                z: 1

                sourceComponent: Composite {
                    session: view.session
                    itemId: view.session.currentId
                }
            }

            CompareView {
                visible: view.comparePath !== ""
                anchors.top: editorBar.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                z: 2
                session: view.session
                itemId: view.session.currentId
                snapshotPath: view.comparePath
                snapshotLabel: view.compareLabel
                onCloseRequested: view.comparePath = ""
            }

            Editor {
                id: editor

                visible: !compositeLoader.show

                anchors.top: editorBar.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                document: view.session.document
                typewriter: view.focusMode
            }

            Item {
                id: editorBar

                visible: !view.focusMode
                width: parent.width
                height: visible ? 48 : 0

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.space.l
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space.xs

                    IconButton {
                        iconName: "maximize-2"
                        label: "Focus mode"
                        onClicked: view.setFocusMode(true)
                    }

                    IconButton {
                        iconName: "panel-right"
                        label: "Inspector"
                        onClicked: view.inspectorOpen = !view.inspectorOpen
                    }
                }
            }
        }

        Inspector {
            visible: view.inspectorOpen && !view.focusMode
            onCompareRequested: (path, label) => {
                view.compareLabel = label;
                view.comparePath = path;
            }
            T.SplitView.preferredWidth: 340
            T.SplitView.minimumWidth: 260
            session: view.session
        }
    }

    // The status bar: the words of the scene, the chapter, and the book.
    Item {
        id: statusBar

        function number(n: int): string {
            return Number(n).toLocaleString(Qt.locale("en_GB"), "f", 0);
        }

        visible: !view.focusMode
        anchors.bottom: parent.bottom
        width: parent.width
        height: visible ? 36 : 0

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
