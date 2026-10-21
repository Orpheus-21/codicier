pragma ComponentBehavior: Bound

import QtQml
import QtQuick
import QtQuick.Dialogs
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
        spellCheck: Settings.spellCheck
        styleChecks: Settings.styleChecks
        hideMarks: Settings.hideMarks
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
        else
            session.currentId = session.items().find(item => item.kind === "scene")?.id ?? "";
        // The writer starts on the page, not in the binder.
        if (session.document.file !== "")
            editor.focusText();
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
            view.noteVisit();
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

    // What the editor area shows: "page", "board" for the corkboard,
    // "outline" for the outliner, "beats" for the beat board, "grid" for the
    // plot grid, "timeline" for the timeline, "relations" for the
    // relationship graph, "family" for the family tree, "insights" for the
    // figures of the manuscript, or "checklist" for the revision checklist.
    property string viewMode: "page"

    function toggleViewMode(mode: string) {
        viewMode = viewMode === mode ? "page" : mode;
    }

    // The kind of the open item, such as "scene" or "map".
    // The board and the outliner show the children of the open item. For an
    // item with no children, they show the children of its parent.
    readonly property string boardId: {
        const id = session.currentId;
        if (id === "" || session.childrenOf(id).length > 0)
            return id;
        return session.fields(id).parent || id;
    }
    readonly property string currentKind: session.currentId !== "" ? (session.fields(session.currentId).kind ?? "") : ""

    // The result of the last action on versions, for the status bar.
    property string versionsMessage: ""

    // A version when the project closes, if the author wants it.
    function versionOnClose() {
        if (Settings.versionOnClose && session.versionsOn)
            session.saveVersion("Closed the project");
    }

    function turnOnVersions() {
        const error = session.turnOnVersions();
        versionsMessage = error !== "" ? error : "Versions are on. The first version is saved.";
    }

    // The items that the author opened, for Alt+Left and Alt+Right, and the
    // cursor of each text. A new item drops the items after the current one.
    property var trail: []
    property int trailAt: -1
    property var cursors: ({})
    property bool walking: false

    // The item on the second page, or "".
    property string sideId: ""

    // Opens a text on the second page. The open text of the main page cannot
    // be there too, because two pages on one file would save over each other.
    function openBeside(id: string) {
        if (id !== session.currentId && (session.fields(id).file ?? "") !== "")
            sideId = id;
    }

    // Closes the second page, or opens the text before this one beside it.
    function toggleBeside() {
        if (sideId !== "")
            sideId = "";
        else if (trailAt > 0)
            openBeside(trail[trailAt - 1]);
    }

    function noteVisit() {
        if (session.currentId === sideId)
            sideId = "";
        const id = session.currentId;
        if (walking || id === "" || trail[trailAt] === id)
            return;
        trail = trail.slice(Math.max(0, trailAt - 98), trailAt + 1).concat([id]);
        trailAt = trail.length - 1;
    }

    function walk(step: int) {
        const at = trailAt + step;
        if (at < 0 || at >= trail.length)
            return;
        const cursor = cursors[trail[at]] ?? 0;
        walking = true;
        trailAt = at;
        viewMode = "page";
        session.currentId = trail[at];
        walking = false;
        Qt.callLater(() => editor.selectRange(cursor, 0));
    }

    // Opens a text from the board or the outliner on the page.
    function openText(id: string) {
        viewMode = "page";
        session.currentId = id;
    }

    // A writing sprint: the end time in milliseconds (0 when no sprint
    // runs), the manuscript words at the start, the seconds left, and the
    // result of the last sprint.
    property double sprintEnd: 0
    property int sprintStartWords: 0
    property int sprintSeconds: 0
    property int sprintLength: 0
    property string sprintResult: ""
    readonly property int sprintWords: Math.max(0, session.manuscriptWords - sprintStartWords)

    function startSprint(seconds: int) {
        sprintStartWords = session.manuscriptWords;
        sprintLength = seconds;
        sprintEnd = Date.now() + seconds * 1000;
        sprintSeconds = seconds;
        sprintResult = "";
        sprintTimer.start();
    }

    function stopSprint() {
        sprintTimer.stop();
        const minutes = Math.round((sprintLength - sprintSeconds) / 60);
        sprintResult = "Sprint: " + sprintWords + (sprintWords === 1 ? " word" : " words") + " in "
                     + minutes + (minutes === 1 ? " minute" : " minutes");
        sprintEnd = 0;
    }

    Timer {
        id: sprintTimer

        interval: 250
        repeat: true
        onTriggered: {
            view.sprintSeconds = Math.max(0, Math.ceil((view.sprintEnd - Date.now()) / 1000));
            if (view.sprintSeconds === 0)
                view.stopSprint();
        }
    }

    // The codex panel and the versions panel also take the place of the
    // binder list.
    property bool codexOpen: false
    property bool versionsOpen: false
    property bool inboxOpen: false

    function openInbox() {
        setFocusMode(false);
        searchOpen = false;
        codexOpen = false;
        versionsOpen = false;
        inboxOpen = true;
        inbox.start();
    }

    // Asks for a comment on the selected words of the page.
    function commentOnSelection() {
        if (session.currentId === "" || editor.selectedText === "")
            return;
        // The page splits paragraphs with U+2029, and the file with a newline.
        commentDialog.ask(session.currentId, editor.selectionStart, editor.selectedText.replace(/\u2029/g, "\n"));
    }

    function openVersions() {
        setFocusMode(false);
        searchOpen = false;
        codexOpen = false;
        inboxOpen = false;
        versionsOpen = true;
        versionsPanel.start();
    }

    function openCodex() {
        setFocusMode(false);
        searchOpen = false;
        versionsOpen = false;
        inboxOpen = false;
        codexOpen = true;
        codex.start();
    }

    // Opens the search panel, or with replace the replace mode of it.
    function openSearch(replace: bool) {
        setFocusMode(false);
        codexOpen = false;
        versionsOpen = false;
        inboxOpen = false;
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
        { id: "format.bold", name: "Bold", sequence: "Ctrl+B", run: () => editor.toggleMark("**") },
        { id: "format.italic", name: "Italic", sequence: "Ctrl+I", run: () => editor.toggleMark("*") },
        { id: "find", name: "Find in the scene", sequence: "Ctrl+F", run: () => editor.openFind(false) },
        { id: "find.replace", name: "Replace in the scene", sequence: "Ctrl+R", run: () => editor.openFind(true) },
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
        { id: "board", name: "Show or hide the corkboard", sequence: "Ctrl+Shift+K", run: () => view.toggleViewMode("board") },
        { id: "calendar", name: "Edit the calendar of the story", sequence: "Ctrl+Alt+C", run: () => calendarDialog.open() },
        { id: "insights", name: "Show or hide the insights", sequence: "Ctrl+Alt+I", run: () => view.toggleViewMode("insights") },
        { id: "family", name: "Show or hide the family tree", sequence: "Ctrl+Alt+F", run: () => view.toggleViewMode("family") },
        { id: "relations", name: "Show or hide the relationship graph", sequence: "Ctrl+Alt+R", run: () => view.toggleViewMode("relations") },
        { id: "timeline", name: "Show or hide the timeline", sequence: "Ctrl+Alt+T", run: () => view.toggleViewMode("timeline") },
        { id: "grid", name: "Show or hide the plot grid", sequence: "Ctrl+Alt+G", run: () => view.toggleViewMode("grid") },
        { id: "beats", name: "Show or hide the beat board", sequence: "Ctrl+Alt+B", run: () => view.toggleViewMode("beats") },
        { id: "outline", name: "Show or hide the outliner", sequence: "Ctrl+Shift+O", run: () => view.toggleViewMode("outline") },
        { id: "sprint", name: "Start or stop a sprint", sequence: "Ctrl+Shift+T", run: () => view.sprintEnd > 0 ? view.stopSprint() : view.startSprint(Settings.sprintMinutes * 60) },
        { id: "comment", name: "Comment on the selected words", sequence: "Ctrl+Alt+K", run: () => view.commentOnSelection() },
        { id: "inbox", name: "Show the open comments", sequence: "Ctrl+Alt+O", run: () => view.openInbox() },
        { id: "checklist", name: "Show or hide the revision checklist", sequence: "Ctrl+Alt+L", run: () => view.toggleViewMode("checklist") },
        { id: "passes", name: "Draft passes", sequence: "Ctrl+Alt+D", run: () => passDialog.open() },
        { id: "versions.list", name: "Show the versions", sequence: "Ctrl+Alt+H", run: () => view.openVersions() },
        { id: "versions.save", name: "Save a version", sequence: "Ctrl+Alt+S", run: () => versionDialog.open() },
        { id: "versions.on", name: "Turn on versions", sequence: "Ctrl+Alt+V", run: () => view.turnOnVersions() },
        { id: "progress", name: "Goals and progress", sequence: "Ctrl+Shift+G", run: () => progressDialog.open() },
        { id: "codex", name: "Open the codex", sequence: "Ctrl+Shift+E", run: () => view.openCodex() },
        { id: "search", name: "Search the project", sequence: "Ctrl+Shift+F", run: () => view.openSearch(false) },
        { id: "replace", name: "Replace in the manuscript", sequence: "Ctrl+H", run: () => view.openSearch(true) },
        { id: "pane.next", name: "Move to the next pane", sequence: "F6", run: () => view.nextPane() },
        { id: "binder", name: "Show or hide the binder", sequence: "Ctrl+Shift+B", run: () => view.binderOpen = !view.binderOpen },
        { id: "inspector", name: "Show or hide the inspector", sequence: "Ctrl+Shift+I", former: "Ctrl+I", run: () => view.inspectorOpen = !view.inspectorOpen },
        { id: "focus", name: "Focus mode", sequence: "F11", run: () => view.setFocusMode(!view.focusMode) },
        { id: "close", name: "Back to the workspace", sequence: "Ctrl+W", run: () => view.close() },
        { id: "help", name: "Show the keys", sequence: "F1", run: () => keySheet.open() },
        { id: "beside", name: "Show or hide the second page", sequence: "Ctrl+\\", run: () => view.toggleBeside() },
        { id: "back", name: "Go back to the text before", sequence: "Alt+Left", run: () => view.walk(-1) },
        { id: "forward", name: "Go forward again", sequence: "Alt+Right", run: () => view.walk(1) }
    ]

    KeyMap {
        id: keyMap
    }

    // The keys from the key map, read when the view opens.
    readonly property var keys: keyMap.resolve(defaultCommands.map(
        command => ({ id: command.id, name: command.name, sequence: command.sequence,
                      former: command.former ?? "" })))
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

    FileDialog {
        id: mapDialog

        title: "Choose the image of a map"
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp *.gif *.bmp)"]
        onAccepted: view.session.addMap(selectedFile)
    }

    CommentDialog {
        id: commentDialog

        parent: T.Overlay.overlay
        anchors.centerIn: parent
        session: view.session
    }

    PassDialog {
        id: passDialog

        parent: T.Overlay.overlay
        anchors.centerIn: parent
        session: view.session
    }

    CalendarDialog {
        id: calendarDialog

        parent: T.Overlay.overlay
        anchors.centerIn: parent
        session: view.session
    }

    VersionDialog {
        id: versionDialog

        parent: T.Overlay.overlay
        anchors.centerIn: parent
        session: view.session
        onSaved: message => view.versionsMessage = message
    }

    KeySheet {
        id: keySheet

        objectName: "keySheet"
        parent: T.Overlay.overlay
        anchors.centerIn: parent
        commands: view.commands
    }

    ProgressDialog {
        id: progressDialog

        sprintRunning: view.sprintEnd > 0
        onSprintRequested: view.sprintEnd > 0 ? view.stopSprint() : view.startSprint(Settings.sprintMinutes * 60)
        parent: T.Overlay.overlay
        anchors.centerIn: parent
        session: view.session
    }

    CommandPalette {
        id: commandPalette

        parent: T.Overlay.overlay
        session: view.session
        commands: view.commands
    }

    // Focus mode dims the text outside the paragraph or the sentence.
    Binding {
        target: view.session.document
        property: "focusScope"
        value: view.focusMode ? Settings.focusScope : 0
    }

    Shortcut {
        sequence: "Escape"
        enabled: view.focusMode
        onActivated: view.setFocusMode(false)
    }

    // Three hints on the first open of a project: the binder, the page, and
    // the command palette. Escape or a click moves to the next one.
    FirstRunHint {
        id: firstRunHint

        objectName: "firstRunHint"
        view: view
        binderWidth: view.binderOpen ? binderPane.width : 0
    }

    Shortcut {
        sequence: "Escape"
        enabled: firstRunHint.visible
        onActivated: firstRunHint.next()
    }

    // Saves the open text, then asks to go back to the workspace.
    function close() {
        view.saveState();
        view.versionOnClose();
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
                T.Action { text: "New map…"; onTriggered: mapDialog.open() }
            }

            Menu {
                id: projectMenu

                T.Action {
                    text: "Search"
                    onTriggered: view.openSearch(false)
                }

                T.Action {
                    enabled: view.session.versionsOn
                    text: "Save a version"
                    onTriggered: versionDialog.open()
                }

                T.Action {
                    enabled: !view.session.versionsOn
                    text: view.session.versionsOn ? "Versions are on" : "Turn on versions"
                    onTriggered: view.turnOnVersions()
                }

                T.Action {
                    text: "Replace in the manuscript"
                    onTriggered: view.openSearch(true)
                }

                T.Action {
                    text: "Scene break: * * *"
                    checked: view.session.sceneBreak === "***"
                    onTriggered: view.session.setSceneBreak("***")
                }

                T.Action {
                    text: "Scene break: #"
                    checked: view.session.sceneBreak === "#"
                    onTriggered: view.session.setSceneBreak("#")
                }

                T.Action {
                    text: "Scene break: a blank line"
                    checked: view.session.sceneBreak === ""
                    onTriggered: view.session.setSceneBreak("")
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

            InboxPanel {
                id: inbox

                objectName: "inboxPanel"
                visible: view.inboxOpen
                anchors.top: projectButton.bottom
                anchors.topMargin: Theme.space.s
                anchors.bottom: parent.bottom
                width: parent.width
                session: view.session
                onCloseRequested: {
                    view.inboxOpen = false;
                    binder.forceActiveFocus();
                }
                onOpened: (id, start, length) => {
                    view.openText(id);
                    editor.selectRange(start, length);
                }
            }

            VersionsPanel {
                id: versionsPanel

                objectName: "versionsPanel"
                visible: view.versionsOpen
                anchors.top: projectButton.bottom
                anchors.topMargin: Theme.space.s
                anchors.bottom: parent.bottom
                width: parent.width
                session: view.session
                onCloseRequested: {
                    view.versionsOpen = false;
                    binder.forceActiveFocus();
                }
                onRestored: message => view.versionsMessage = message
            }

            Binder {
                id: binder

                visible: !view.searchOpen && !view.codexOpen && !view.versionsOpen && !view.inboxOpen
                // Enter on a text item moves the focus to the page.
                onActivated: editor.focusText()
                onBesideRequested: id => view.openBeside(id)
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

            MapView {
                objectName: "mapView"
                visible: view.viewMode === "page" && view.currentKind === "map"
                anchors.top: editorBar.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                z: 3
                session: view.session
                itemId: visible ? view.session.currentId : ""
                onOpened: id => view.openText(id)
            }

            Corkboard {
                objectName: "corkboardView"
                visible: view.viewMode === "board"
                anchors.top: editorBar.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                z: 4
                session: view.session
                itemId: visible ? view.boardId : ""
                onOpened: id => view.openText(id)
            }

            Checklist {
                objectName: "checklistView"
                visible: view.viewMode === "checklist"
                anchors.top: editorBar.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                z: 4
                session: view.session
            }

            Insights {
                objectName: "insightsView"
                visible: view.viewMode === "insights"
                anchors.top: editorBar.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                z: 4
                session: view.session
                onOpened: id => view.openText(id)
            }

            FamilyTree {
                objectName: "familyTree"
                visible: view.viewMode === "family"
                anchors.top: editorBar.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                z: 4
                session: view.session
                onOpened: id => view.openText(id)
            }

            RelationGraph {
                objectName: "relationGraph"
                visible: view.viewMode === "relations"
                anchors.top: editorBar.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                z: 4
                session: view.session
                onOpened: id => view.openText(id)
            }

            Timeline {
                objectName: "timelineView"
                visible: view.viewMode === "timeline"
                anchors.top: editorBar.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                z: 4
                session: view.session
                onOpened: id => view.openText(id)
                onCalendarRequested: calendarDialog.open()
            }

            PlotGrid {
                objectName: "plotGridView"
                visible: view.viewMode === "grid"
                anchors.top: editorBar.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                z: 4
                session: view.session
            }

            BeatBoard {
                objectName: "beatBoard"
                visible: view.viewMode === "beats"
                anchors.top: editorBar.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                z: 4
                session: view.session
                onOpened: id => view.openText(id)
            }

            Outliner {
                objectName: "outlinerView"
                visible: view.viewMode === "outline"
                anchors.top: editorBar.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                z: 4
                session: view.session
                itemId: visible ? view.boardId : ""
                onOpened: id => view.openText(id)
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
                width: sidePage.visible ? parent.width / 2 : parent.width
                document: view.session.document
                session: view.session
                typewriter: view.focusMode
                onLinkOpened: id => view.session.currentId = id
                onCursorPositionChanged: {
                    // A load moves the cursor too. Keep only the moves of the author.
                    if (!view.session.document.isLoading() && view.session.currentId !== "")
                        view.cursors[view.session.currentId] = cursorPosition;
                }
            }

            SidePage {
                id: sidePage

                objectName: "sidePage"
                visible: view.sideId !== "" && view.viewMode === "page"
                anchors.top: editorBar.bottom
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                width: parent.width / 2
                session: view.session
                itemId: view.sideId
                onCloseRequested: view.sideId = ""
            }

            Item {
                id: editorBar

                visible: !view.focusMode
                width: parent.width
                height: visible ? 48 : 0

                IconButton {
                    objectName: "backToWorkspace"
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space.l
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "arrow-left"
                    label: "Back to the workspace"
                    keys: view.keys.sequences["close"] ?? ""
                    showLabel: Settings.buttonLabels
                    onClicked: view.close()
                }

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.space.l
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space.xs

                    IconButton {
                        iconName: "list-tree"
                        label: "Outliner"
                        keys: view.keys.sequences["outline"] ?? ""
                        showLabel: Settings.buttonLabels
                        onClicked: view.toggleViewMode("outline")
                    }

                    IconButton {
                        iconName: "layout-grid"
                        label: "Corkboard"
                        keys: view.keys.sequences["board"] ?? ""
                        showLabel: Settings.buttonLabels
                        onClicked: view.toggleViewMode("board")
                    }

                    IconButton {
                        iconName: "panel-left"
                        label: "Binder"
                        keys: view.keys.sequences["binder"] ?? ""
                        showLabel: Settings.buttonLabels
                        onClicked: view.binderOpen = !view.binderOpen
                    }

                    IconButton {
                        iconName: "maximize-2"
                        label: "Focus mode"
                        keys: view.keys.sequences["focus"] ?? ""
                        showLabel: Settings.buttonLabels
                        onClicked: view.setFocusMode(true)
                    }

                    IconButton {
                        iconName: "panel-right"
                        label: "Inspector"
                        keys: view.keys.sequences["inspector"] ?? ""
                        showLabel: Settings.buttonLabels
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
                objectName: "selectionCount"
                visible: editor.selectedText !== ""
                text: "Selection " + statusBar.number(view.session.document.wordsIn(editor.selectedText))
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

            // The session words, against the session target if there is one.
            Row {
                objectName: "sessionCount"
                visible: view.session.error === ""
                spacing: Theme.space.s

                Count {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Session " + statusBar.number(view.session.sessionWords)
                          + (Settings.sessionTarget > 0 ? " of " + statusBar.number(Settings.sessionTarget) : "")
                }

                Rectangle {
                    visible: Settings.sessionTarget > 0
                    anchors.verticalCenter: parent.verticalCenter
                    width: 72
                    height: 4
                    radius: 2
                    color: Theme.color.rule

                    Rectangle {
                        width: parent.width * Math.min(1, view.session.sessionWords / Math.max(1, Settings.sessionTarget))
                        height: parent.height
                        radius: 2
                        color: Theme.color.accent
                    }
                }
            }

            Count {
                objectName: "sprintStatus"
                visible: view.sprintEnd > 0 || view.sprintResult !== ""
                text: view.sprintEnd > 0
                      ? "Sprint " + Math.floor(view.sprintSeconds / 60) + ":"
                        + String(view.sprintSeconds % 60).padStart(2, "0") + ", " + statusBar.number(view.sprintWords) + " words"
                      : view.sprintResult
                color: view.sprintEnd > 0 ? Theme.color.accent : Theme.color.inkMuted
            }

            Count {
                visible: view.session.dailyNeeded >= 0
                text: statusBar.number(view.session.dailyNeeded) + " a day to the deadline"
            }

            Count {
                objectName: "versionsMessage"
                visible: view.versionsMessage !== ""
                text: view.versionsMessage
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

        // The save state of the open text, at the right end. An error stays
        // until a save works.
        Text {
            objectName: "saveState"
            visible: view.session.document.file !== ""
            anchors.right: parent.right
            anchors.rightMargin: Theme.space.l
            anchors.verticalCenter: parent.verticalCenter
            text: view.session.document.error !== "" ? "Not saved"
                : view.session.document.dirty ? "Saving" : "Saved"
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            color: view.session.document.error !== "" ? Theme.color.accent : Theme.color.inkMuted
            Accessible.role: Accessible.StaticText
            Accessible.name: view.session.document.error !== "" ? view.session.document.error : text
        }
    }
}
