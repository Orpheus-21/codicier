pragma ComponentBehavior: Bound

import QtQml
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

    // The binder pane on the left and the inspector pane on the right.
    property bool binderOpen: true
    property bool inspectorOpen: false

    // The layout and the open item come back from .codicier/state.toml.
    function restoreState() {
        const state = session.viewState();
        if (typeof state.binder === "boolean")
            binderOpen = state.binder;
        if (typeof state.inspector === "boolean")
            inspectorOpen = state.inspector;
        if (state["binder-width"] > 0)
            binderPane.T.SplitView.preferredWidth = state["binder-width"];
        if (state["inspector-width"] > 0)
            inspectorPane.T.SplitView.preferredWidth = state["inspector-width"];
        if (state.item && session.fields(state.item).title !== undefined)
            session.currentId = state.item;
    }

    function saveState() {
        stateTimer.stop();
        session.saveViewState({
            "item": session.currentId,
            "binder": binderOpen,
            "binder-width": Math.round(binderPane.T.SplitView.preferredWidth),
            "inspector": inspectorOpen,
            "inspector-width": Math.round(inspectorPane.T.SplitView.preferredWidth)
        });
    }

    // A change of the layout saves the state half a second later.
    Timer {
        id: stateTimer

        interval: 500
        onTriggered: view.saveState()
    }

    onBinderOpenChanged: stateTimer.restart()
    onInspectorOpenChanged: stateTimer.restart()
    Component.onCompleted: restoreState()
    Component.onDestruction: saveState()
    // The snapshot that the compare view shows, or "".
    property string comparePath: ""
    property string compareLabel: ""

    Connections {
        target: view.session

        function onCurrentIdChanged() {
            view.comparePath = "";
            stateTimer.restart();
            // A saved search opens its results.
            const fields = view.session.fields(view.session.currentId);
            if (fields.kind === "search") {
                view.openSearch(false);
                search.show(fields.query, fields.regex);
            }
        }
    }

    // The search panel takes the place of the binder list.
    property bool searchOpen: false

    // The codex panel also takes the place of the binder list.
    property bool codexOpen: false

    function openCodex() {
        setFocusMode(false);
        searchOpen = false;
        codexOpen = true;
        codex.start();
    }

    // Opens the search panel, or with replace the replace mode of it.
    function openSearch(replace: bool) {
        setFocusMode(false);
        codexOpen = false;
        searchOpen = true;
        search.replacing = replace;
        search.start();
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
    function split() {
        const id = session.split(session.currentId, editor.cursorPosition);
        if (id !== "")
            binder.startRename(id);
    }

    // Moves the focus to the next open pane: the binder, the page, then the
    // inspector. Tab stays a key of the text, so F6 reaches the page.
    function nextPane() {
        const focused = Window.activeFocusItem;
        const inside = pane => {
            for (let item = focused; item; item = item.parent) {
                if (item === pane)
                    return true;
            }
            return false;
        };
        let panes = [];
        if (binderPane.visible)
            panes.push(binderPane);
        panes.push(editor);
        if (inspectorPane.visible)
            panes.push(inspectorPane);
        const at = panes.findIndex(inside);
        const next = panes[(at + 1) % panes.length];
        if (next === editor)
            editor.focusText();
        else if (next === binderPane)
            binder.forceActiveFocus();
        else
            inspectorPane.nextItemInFocusChain(true).forceActiveFocus();
    }

    // Every command of the view, with its default key. The key map in
    // keys.ini can change each key. The command palette lists the commands,
    // except a hidden one, and each command with a key gets a shortcut.
    readonly property var defaultCommands: [
        { id: "palette", name: "Find a command or an item", sequence: "Ctrl+Shift+P", run: () => commandPalette.open(), hidden: true },
        { id: "scene.new", name: "New scene", sequence: "Ctrl+N", run: () => binder.addItem("scene") },
        { id: "chapter.new", name: "New chapter", sequence: "Ctrl+Shift+N", run: () => binder.addItem("chapter") },
        { id: "part.new", name: "New part", sequence: "Ctrl+Alt+N", run: () => binder.addItem("part") },
        { id: "note.new", name: "New note", sequence: "Ctrl+Alt+M", run: () => binder.addItem("note") },
        { id: "page.new", name: "New page", sequence: "Ctrl+Alt+P", run: () => binder.addItem("page") },
        { id: "item.rename", name: "Rename the item", sequence: "F2", run: () => binder.startRename(session.currentId) },
        { id: "scene.split", name: "Split the scene at the cursor", sequence: "Ctrl+K", run: () => view.split() },
        { id: "scene.merge", name: "Merge with the next scene", sequence: "Ctrl+Shift+J", run: () => session.mergeWithNext(session.currentId) },
        { id: "item.trash", name: "Move the item to the trash", sequence: "Ctrl+Shift+Del", run: () => session.moveToTrash(session.currentId) },
        { id: "snapshot.take", name: "Take a snapshot", sequence: "Ctrl+Shift+S", run: () => session.takeSnapshot(session.currentId, "") },
        { id: "codex", name: "Open the codex", sequence: "Ctrl+Shift+E", run: () => view.openCodex() },
        { id: "search", name: "Search the project", sequence: "Ctrl+Shift+F", run: () => view.openSearch(false) },
        { id: "replace", name: "Replace in the manuscript", sequence: "Ctrl+H", run: () => view.openSearch(true) },
        { id: "pane.next", name: "Move to the next pane", sequence: "F6", run: () => view.nextPane() },
        { id: "binder", name: "Show or hide the binder", sequence: "Ctrl+Shift+B", run: () => view.binderOpen = !view.binderOpen },
        { id: "inspector", name: "Show or hide the inspector", sequence: "Ctrl+I", run: () => view.inspectorOpen = !view.inspectorOpen },
        { id: "focus", name: "Focus mode", sequence: "F11", run: () => view.setFocusMode(!view.focusMode) },
        { id: "close", name: "Back to the workspace", sequence: "Ctrl+W", run: () => view.close() }
    ]

    KeyMap {
        id: keyMap
    }

    // The keys from the key map, read when the view opens.
    readonly property var keys: keyMap.resolve(defaultCommands.map(
        command => ({ id: command.id, name: command.name, sequence: command.sequence })))
    readonly property var commands: defaultCommands.map(
        command => Object.assign({}, command, { sequence: keys.sequences[command.id] }))

    Instantiator {
        model: view.commands

        delegate: Shortcut {
            required property var modelData

            sequence: modelData.sequence
            enabled: modelData.sequence !== ""
            onActivated: modelData.run()
        }
    }

    CommandPalette {
        id: commandPalette

        parent: T.Overlay.overlay
        session: view.session
        commands: view.commands
    }

    Shortcut {
        sequence: "Escape"
        enabled: view.focusMode
        onActivated: view.setFocusMode(false)
    }

    // Saves the open text, then asks to go back to the workspace.
    function close() {
        view.saveState();
        view.session.backup();
        view.closeRequested();
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
            id: binderPane

            objectName: "binderPane"
            visible: view.binderOpen && !view.focusMode
            T.SplitView.onPreferredWidthChanged: stateTimer.restart()
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
                T.Action { text: "New character"; onTriggered: binder.addItem("character") }
                T.Action { text: "New place"; onTriggered: binder.addItem("place") }
                T.Action { text: "New item"; onTriggered: binder.addItem("item") }
                T.Action { text: "New faction"; onTriggered: binder.addItem("faction") }
                T.Action { text: "New lore"; onTriggered: binder.addItem("lore") }
                T.Action { text: "New term"; onTriggered: binder.addItem("term") }
            }

            Menu {
                id: projectMenu

                T.Action {
                    text: "Search"
                    onTriggered: view.openSearch(false)
                }

                T.Action {
                    text: "Replace in the manuscript"
                    onTriggered: view.openSearch(true)
                }

                T.Action {
                    text: "Back to " + view.workspaceName
                    onTriggered: view.close()
                }
            }

            SearchPanel {
                id: search

                visible: view.searchOpen
                anchors.top: projectButton.bottom
                anchors.topMargin: Theme.space.s
                anchors.bottom: parent.bottom
                width: parent.width
                session: view.session
                onCloseRequested: {
                    view.searchOpen = false;
                    binder.forceActiveFocus();
                }
                onOpened: (id, query, regex) => {
                    view.session.currentId = id;
                    const match = view.session.matchIn(id, query, regex);
                    editor.selectRange(match.start, match.length);
                }
            }

            CodexPanel {
                id: codex

                objectName: "codexPanel"
                visible: view.codexOpen
                anchors.top: projectButton.bottom
                anchors.topMargin: Theme.space.s
                anchors.bottom: parent.bottom
                width: parent.width
                session: view.session
                onCloseRequested: {
                    view.codexOpen = false;
                    binder.forceActiveFocus();
                }
                onOpened: id => {
                    view.session.currentId = id;
                    editor.focusText();
                }
                // The new entity starts with a rename in the binder.
                onAddRequested: kind => {
                    view.codexOpen = false;
                    binder.addItem(kind);
                }
            }

            Binder {
                id: binder

                visible: !view.searchOpen && !view.codexOpen
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
                session: view.session
                typewriter: view.focusMode
                onLinkOpened: id => view.session.currentId = id
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
                        iconName: "panel-left"
                        label: "Binder"
                        onClicked: view.binderOpen = !view.binderOpen
                    }

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
            id: inspectorPane

            visible: view.inspectorOpen && !view.focusMode
            T.SplitView.onPreferredWidthChanged: stateTimer.restart()
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
                objectName: "keyWarnings"
                visible: view.keys.warnings.length > 0
                text: "Key map: " + view.keys.warnings.join(" ")
                color: Theme.color.accent
            }

            Count {
                visible: view.session.readOnly
                text: "Read only: this project uses a newer format."
            }
        }
    }
}
