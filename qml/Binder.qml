pragma ComponentBehavior: Bound

import QtQuick

// The binder list of a project. Up and Down move, Left closes, Right opens,
// and Enter or a click opens the item.
ListView {
    id: binder

    required property ProjectSession session

    // Enter opened an item. The view moves the focus to the page.
    signal activated

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
