pragma ComponentBehavior: Bound

import QtQuick

// The children of an item as index cards: the title, the status, the
// synopsis, and the words. A drag of a card onto another card moves the item
// in the binder. A click opens the item.
Item {
    id: board

    required property ProjectSession session
    required property string itemId

    // A click on a card asks to open the item.
    signal opened(id: string)

    property var cards: []
    // The card that the drag holds, and the card under the pointer.
    property string dragId: ""
    property int dropIndex: -1

    function refresh() {
        cards = itemId === "" ? [] : session.childrenOf(itemId);
    }

    // Moves the dragged card before the card under it, or after the last card.
    function drop(index: int) {
        if (dragId !== "" && index >= 0 && cards[index].id !== dragId) {
            const from = cards.findIndex(card => card.id === dragId);
            session.move(dragId, cards[index].id, from < index ? "after" : "before");
        }
        dragId = "";
        dropIndex = -1;
    }

    onItemIdChanged: refresh()
    Component.onCompleted: refresh()

    Connections {
        target: board.session

        function onEdited() {
            board.refresh();
        }

        // The live count of the open text arrives after the typing stops.
        function onWordsChanged() {
            if (board.visible)
                board.refresh();
        }
    }

    // The board has the colour of the page, so the splitter handle shows no
    // strip. The cards stand out in the surface colour.
    Rectangle {
        anchors.fill: parent
        color: Theme.color.paper
    }

    Text {
        visible: board.cards.length === 0
        anchors.centerIn: parent
        text: "This item holds no other items."
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    GridView {
        id: grid

        objectName: "corkboard"
        anchors.fill: parent
        anchors.margins: Theme.space.xl
        cellWidth: 248
        cellHeight: 176
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: board.cards

        WheelHandler {
            onWheel: event => {
                const end = Math.max(0, grid.contentHeight - grid.height);
                grid.contentY = Math.min(end, Math.max(0, grid.contentY - event.angleDelta.y / 2));
            }
        }

        delegate: Item {
            id: cell

            required property var modelData
            required property int index

            width: grid.cellWidth
            height: grid.cellHeight

            Rectangle {
                id: card

                objectName: "card"
                x: Theme.space.s
                y: Theme.space.s
                width: parent.width - 2 * Theme.space.s
                height: parent.height - 2 * Theme.space.s
                radius: Theme.radius.small
                color: Theme.color.surface
                border.width: board.dropIndex === cell.index && board.dragId !== cell.modelData.id ? 2 : 1
                border.color: board.dropIndex === cell.index && board.dragId !== cell.modelData.id
                              ? Theme.color.accent : Theme.color.rule
                opacity: board.dragId === cell.modelData.id ? 0.5 : 1
                Accessible.role: Accessible.ListItem
                Accessible.name: cell.modelData.title

                // The colour label: a stripe along the top of the card.
                Rectangle {
                    width: parent.width
                    height: 4
                    radius: Theme.radius.small
                    color: Theme.labelColor(board.session.fields(cell.modelData.id).label ?? "")
                }

                Column {
                    x: Theme.space.m
                    y: Theme.space.m
                    width: parent.width - 2 * Theme.space.m
                    spacing: Theme.space.xs

                    Text {
                        width: parent.width
                        text: cell.modelData.title
                        elide: Text.ElideRight
                        font.family: Theme.type.reading
                        font.pixelSize: Theme.type.large
                        color: Theme.color.ink
                    }

                    Text {
                        width: parent.width
                        height: card.height - 2 * Theme.space.m - 2 * Theme.type.large - Theme.space.s
                        text: cell.modelData.synopsis !== "" ? cell.modelData.synopsis : "No synopsis"
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                        font.family: Theme.type.reading
                        font.italic: true
                        font.pixelSize: Theme.type.body
                        color: cell.modelData.synopsis !== "" ? Theme.color.ink : Theme.color.inkMuted
                    }
                }

                Text {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: Theme.space.m
                    text: cell.modelData.status !== "" ? cell.modelData.status : cell.modelData.kind
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.small
                    font.capitalization: Font.Capitalize
                    color: Theme.color.inkMuted
                }

                Text {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: Theme.space.m
                    text: Number(cell.modelData.words).toLocaleString(Qt.locale("en_GB"), "f", 0) + " words"
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.small
                    color: Theme.color.inkMuted
                }

                // A click with no drag opens the item.
                TapHandler {
                    onTapped: board.opened(cell.modelData.id)
                }

                DragHandler {
                    target: null
                    onActiveChanged: {
                        if (active) {
                            board.dragId = cell.modelData.id;
                        } else {
                            board.drop(board.dropIndex);
                        }
                    }
                    onCentroidChanged: {
                        if (!active)
                            return;
                        const point = card.mapToItem(grid.contentItem, centroid.position.x, centroid.position.y);
                        board.dropIndex = grid.indexAt(point.x, point.y);
                    }
                }
            }
        }
    }
}
