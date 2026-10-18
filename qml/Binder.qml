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

    Keys.onPressed: event => {
        const id = idAt(currentIndex);
        if (event.key === Qt.Key_F2) {
            startRename(id);
            event.accepted = true;
        } else if (event.key === Qt.Key_Delete && id !== "" && !id.startsWith("section:")) {
            session.moveToTrash(id);
            event.accepted = true;
        }
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
        T.Action { text: "Move to trash"; onTriggered: binder.session.moveToTrash(binder.menuId) }
    }

    // The menu for an item in the trash.
    Menu {
        id: trashMenu

        T.Action { text: "Restore"; onTriggered: binder.session.restore(binder.menuId) }
    }

    model: session.binder
    clip: true
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
        current: itemId === binder.session.currentId
        renaming: itemId === binder.renamingId
        onRenameAccepted: newTitle => {
            const id = itemId;
            binder.renamingId = "";
            binder.session.rename(id, newTitle);
            binder.forceActiveFocus();
        }
        onRenameCancelled: binder.renamingId = ""
        onMenuRequested: position => {
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
            if (toggles)
                binder.session.binder.toggle(row);
        }
    }
}
