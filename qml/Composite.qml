pragma ComponentBehavior: Bound

import QtQuick

// All the text items below a chapter or a part, as one scroll. Each scene
// keeps its own file: a title in small capitals, then its text.
ListView {
    id: composite

    required property ProjectSession session
    // The container to show.
    required property string itemId
    // Each scene saves to its own file one second after an edit.
    property bool readOnly: false

    // Moves the keyboard focus into the block at index, at its start or end.
    // Returns false if there is no block there.
    function focusBlock(index: int, atStart: bool): bool {
        if (index < 0 || index >= count)
            return false;
        positionViewAtIndex(index, ListView.Contain);
        const block = itemAtIndex(index) as CompositeBlock;
        if (!block)
            return false;
        block.text.forceActiveFocus();
        block.text.cursorPosition = atStart ? 0 : block.text.length;
        return true;
    }

    objectName: "composite"
    model: session.textsUnder(itemId)
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    topMargin: Theme.space.xxxl
    bottomMargin: Theme.space.xxxl
    spacing: Theme.space.xxl
    reuseItems: false

    // A binder edit, such as a new scene in this chapter, reads the list
    // again.
    Connections {
        target: composite.session

        function onEdited() {
            composite.model = composite.session.textsUnder(composite.itemId);
        }
    }

    delegate: CompositeBlock {
        width: ListView.view.width
        session: composite.session
        readOnly: composite.readOnly
        // Up on the first line and Down on the last line cross the border.
        onLeaveUp: composite.focusBlock(index - 1, false)
        onLeaveDown: composite.focusBlock(index + 1, true)
    }
}
