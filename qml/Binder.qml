pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// The binder list of a project. Up and Down move, Left closes, Right opens,
// and Enter or a click opens the item.
ListView {
    id: binder

    required property ProjectSession session

    // Enter opened an item. The view moves the focus to the page.
    signal activated

    // The id of the row in rename mode, or "".
    property string renamingId: ""
    // The id of the row that the menu is for.
    property string menuId: ""

    // Adds an item of kind near the selection and starts its rename.
    function addItem(kind: string) {
        const id = session.add(kind);
        if (id !== "")
            startRename(id);
    }

    function startRename(id: string) {
        const row = session.binder.rowOf(id);
        if (row < 0 || id.startsWith("section:"))
            return;
        positionViewAtIndex(row, ListView.Contain);
        renamingId = id;
    }

    function idAt(row: int): string {
        return row < 0 ? "" : model.data(model.index(row, 0), BinderModel.IdRole);
    }

    // With no current row, the focus stays on the list itself, and a screen
    // reader reads nothing. The first row becomes current. A rename field in
    // a row also gives the list active focus, so only the list counts.
    onActiveFocusChanged: {
        if (Window.activeFocusItem === binder && currentIndex < 0 && count > 0)
            currentIndex = 0;
    }

    Keys.onPressed: event => {
        const id = idAt(currentIndex);
        if ((event.key === Qt.Key_Up || event.key === Qt.Key_Down)
                && (event.modifiers & Qt.AltModifier)) {
            if (session.moveBy(id, event.key === Qt.Key_Up ? -1 : 1))
                Qt.callLater(() => binder.currentIndex = session.binder.rowOf(id));
            event.accepted = true;
        } else if (event.key === Qt.Key_F2) {
            startRename(id);
            event.accepted = true;
        } else if (event.key === Qt.Key_Delete && id !== "" && !id.startsWith("section:")) {
            session.moveToTrash(id);
            event.accepted = true;
        }
    }

    WheelHandler {
        onWheel: event => {
            const end = Math.max(0, binder.contentHeight - binder.height);
            binder.contentY = Math.min(end, Math.max(0, binder.contentY - event.angleDelta.y / 2));
        }
    }

    // Drag and drop: the dragged item, the row under the pointer, and where
    // the item goes there ("before", "after", or "inside").
    property string dragId: ""
    property int dropRow: -1
    property string dropPosition: ""

    function dragTo(y: real) {
        const row = indexAt(0, y + contentY);
        const target = idAt(row);
        let position = "";
        if (row >= 0 && !target.startsWith("section:")) {
            const item = itemAtIndex(row);
            const part = (y + contentY - item.y) / item.height;
            if (part > 0.25 && part < 0.75 && session.canMove(dragId, target, "inside"))
                position = "inside";
            else if (part <= 0.5 && session.canMove(dragId, target, "before"))
                position = "before";
            else if (session.canMove(dragId, target, "after"))
                position = "after";
        }
        dropRow = position === "" ? -1 : row;
        dropPosition = position;
    }

    function drop() {
        if (dragId !== "" && dropRow >= 0)
            session.move(dragId, idAt(dropRow), dropPosition);
        dragId = "";
        dropRow = -1;
        dropPosition = "";
    }

    // The accent line where the item lands, or a frame for "inside".
    Rectangle {
        readonly property BinderRow target: binder.dropRow >= 0 ? binder.itemAtIndex(binder.dropRow) as BinderRow : null

        visible: target !== null
        z: 2
        x: target ? target.leftPadding + (binder.dropPosition === "inside" ? 0 : 14) : 0
        y: !target ? 0 : binder.dropPosition === "after" ? target.y + target.height - binder.contentY
                     : target.y - binder.contentY - (binder.dropPosition === "before" ? 1 : 0)
        width: target ? binder.width - x : 0
        height: binder.dropPosition === "inside" && target ? target.height : 2
        radius: binder.dropPosition === "inside" ? Theme.radius.small : 1
        color: binder.dropPosition === "inside" ? "transparent" : Theme.color.accent
        border.width: binder.dropPosition === "inside" ? 1.5 : 0
        border.color: Theme.color.accent
    }

    // The menu for an item outside the trash.
    Menu {
        id: itemMenu

        T.Action { text: "New scene"; onTriggered: binder.addItem("scene") }
        T.Action { text: "New chapter"; onTriggered: binder.addItem("chapter") }
        T.Action { text: "New part"; onTriggered: binder.addItem("part") }
        T.Action { text: "New note"; onTriggered: binder.addItem("note") }
        T.Action { text: "New page"; onTriggered: binder.addItem("page") }
        T.Action { text: "Rename"; onTriggered: binder.startRename(binder.menuId) }
        T.Action {
            text: "Merge with next scene"
            enabled: binder.menuId !== "" && binder.session.nextScene(binder.menuId) !== ""
            onTriggered: binder.session.mergeWithNext(binder.menuId)
        }
        T.Action { text: "Move to trash"; onTriggered: binder.session.moveToTrash(binder.menuId) }
    }

    // The menu of the trash heading.
    Menu {
        id: trashSectionMenu

        T.Action {
            text: "Empty trash"
            enabled: binder.session.trashCount() > 0
            onTriggered: emptyTrashDialog.open()
        }
    }

    // One confirm before the files leave the disk.
    T.Popup {
        id: emptyTrashDialog

        parent: T.Overlay.overlay
        anchors.centerIn: parent
        implicitWidth: 440
        implicitHeight: implicitContentHeight + topPadding + bottomPadding
        padding: Theme.space.xl
        modal: true
        focus: true

        T.Overlay.modal: Rectangle {
            color: Qt.alpha(Theme.color.ink, 0.25)
        }

        background: Rectangle {
            radius: Theme.radius.medium
            color: Theme.color.paper
            border.width: 1
            border.color: Theme.color.rule
        }

        contentItem: Column {
            spacing: Theme.space.l

            Text {
                text: "Empty the trash?"
                font.family: Theme.type.reading
                font.pixelSize: Theme.type.title
                color: Theme.color.ink
            }

            Text {
                width: parent.width
                wrapMode: Text.Wrap
                text: "Delete " + binder.session.trashCount() + (binder.session.trashCount() === 1 ? " item" : " items")
                      + " for good? Their files and snapshots leave the disk. A backup is written first."
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.body
                color: Theme.color.inkMuted
            }

            Row {
                anchors.right: parent.right
                spacing: Theme.space.s

                Button {
                    text: "Cancel"
                    onClicked: emptyTrashDialog.close()
                }

                Button {
                    objectName: "confirmEmptyTrash"
                    kind: "primary"
                    text: "Delete"
                    onClicked: {
                        binder.session.emptyTrash();
                        emptyTrashDialog.close();
                    }
                }
            }
        }
    }

    // The menu for an item in the trash.
    Menu {
        id: trashMenu

        T.Action { text: "Restore"; onTriggered: binder.session.restore(binder.menuId) }
    }

    model: session.binder
    clip: true
    // A drag on a row moves the item, so the list itself does not flick. The
    // wheel scrolls it.
    interactive: false
    spacing: 2
    keyNavigationEnabled: true
    boundsBehavior: Flickable.StopAtBounds
    currentIndex: -1

    function open(row: int) {
        binder.currentIndex = row;
        const id = binder.model.data(binder.model.index(row, 0), BinderModel.IdRole);
        if (!id.startsWith("section:"))
            binder.session.currentId = id;
    }

    Keys.onLeftPressed: binder.session.binder.setExpanded(currentIndex, false)
    Keys.onRightPressed: binder.session.binder.setExpanded(currentIndex, true)
    Keys.onReturnPressed: {
        open(currentIndex);
        if (session.document.file !== "")
            activated();
    }

    // The model resets on each open or close. Keep the current row on the
    // same item or section.
    property string keptKey: ""

    Connections {
        target: binder.session.binder

        function onModelAboutToBeReset() {
            binder.keptKey = binder.currentIndex < 0 ? ""
                : binder.model.data(binder.model.index(binder.currentIndex, 0), BinderModel.IdRole);
        }

        function onModelReset() {
            const key = binder.keptKey;
            Qt.callLater(() => binder.currentIndex = binder.session.binder.rowOf(key));
        }
    }

    delegate: BinderRow {
        width: ListView.view.width
        // The current row holds the focus inside the list, so a screen
        // reader reads each row as the arrow keys move. The keys that the
        // row does not use go on to the list.
        focus: ListView.isCurrentItem
        current: itemId === binder.session.currentId
        renaming: itemId === binder.renamingId
        onRenameAccepted: newTitle => {
            const id = itemId;
            binder.renamingId = "";
            binder.session.rename(id, newTitle);
            binder.forceActiveFocus();
        }
        onRenameCancelled: binder.renamingId = ""
        dragged: itemId === binder.dragId
        onDragStarted: binder.dragId = itemId
        onDragMoved: y => binder.dragTo(mapToItem(binder, 0, y).y)
        onDragEnded: binder.drop()
        onMenuRequested: position => {
            if (itemId === "section:trash") {
                trashSectionMenu.popup(this, position.x, position.y);
                return;
            }
            if (isSection)
                return;
            binder.menuId = itemId;
            binder.session.currentId = itemId;
            const menu = binder.session.binder.isInTrash(itemId) ? trashMenu : itemMenu;
            menu.popup(this, position.x, position.y);
        }
        onToggleRequested: binder.session.binder.toggle(index)
        onClicked: {
            // A toggle resets the model and ends this delegate, so read the
            // values first and toggle last.
            const row = index;
            const opens = !isSection;
            const toggles = isSection || hasChildren;
            binder.forceActiveFocus();
            if (opens)
                binder.open(row);
            // A click on a text moves the focus to the page, as Enter does.
            if (opens && !toggles && binder.session.document.file !== "")
                binder.activated();
            if (toggles)
                binder.session.binder.toggle(row);
        }
    }
}
