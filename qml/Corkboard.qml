pragma ComponentBehavior: Bound

import QtQuick

// The children of an item as index cards: the title, the status, the
// synopsis, and the words. A drag of a card onto another card moves the item
// in the binder. A click opens the item.
Item {
    id: board

    Accessible.role: Accessible.Pane
    Accessible.name: "Corkboard"

    required property ProjectSession session
    // The board shows every scene of the book, not the open chapter.
    property bool wholeBook: false
    signal scopeChosen(bool whole)
    required property string itemId

    // A click on a card asks to open the item.
    signal opened(id: string)

    property var cards: []
    // The point of view characters of the whole manuscript, in order of
    // first use, so each one has the same colour on every board.
    property var allPovs: []
    // The ones on this board, for the legend.
    readonly property var povs: allPovs.filter(pov => cards.some(card => (session.fields(card.id).pov ?? "") === pov))

    function povColor(pov: string): color {
        const at = allPovs.indexOf(pov);
        return at < 0 ? "transparent" : Theme.color.series[at % Theme.color.series.length];
    }

    // The colour of a card: its label, or the colour of its point of view.
    function cardColor(id: string): color {
        return Settings.cardColour === "pov" ? povColor(session.fields(id).pov ?? "")
                                             : Theme.labelColor(session.fields(id).label ?? "");
    }
    // The card that the drag holds, and the card under the pointer.
    property string dragId: ""
    property int dropIndex: -1

    function refresh() {
        allPovs = session.items().filter(item => item.kind === "scene")
            .map(item => session.fields(item.id).pov ?? "")
            .filter((pov, at, all) => pov !== "" && all.indexOf(pov) === at);
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

    // One index card: the title, the synopsis, the status, and the words.
    component CardFace: Rectangle {
        id: face

        required property var item
        // True while a card of the grid hovers over this one.
        property bool marked: false

        radius: Theme.radius.small
        color: Theme.color.surface
        border.width: marked ? 2 : 1
        border.color: marked ? Theme.color.accent : Theme.color.rule
        Accessible.role: Accessible.ListItem
        Accessible.name: item.title

        // The colour label: a stripe along the top of the card.
        Rectangle {
            width: parent.width
            height: 4
            radius: Theme.radius.small
            color: board.cardColor(face.item.id)
        }

        Column {
            x: Theme.space.m
            y: Theme.space.m
            width: parent.width - 2 * Theme.space.m
            spacing: Theme.space.xs

            Text {
                width: parent.width
                text: face.item.title
                elide: Text.ElideRight
                font.family: Theme.type.reading
                font.pixelSize: Theme.type.large
                color: Theme.color.ink
            }

            Text {
                width: parent.width
                height: face.height - 2 * Theme.space.m - 2 * Theme.type.large - Theme.space.s
                text: face.item.synopsis !== "" ? face.item.synopsis : "No synopsis"
                wrapMode: Text.Wrap
                elide: Text.ElideRight
                font.family: Theme.type.reading
                font.italic: true
                font.pixelSize: Theme.type.body
                color: face.item.synopsis !== "" ? Theme.color.ink : Theme.color.inkMuted
            }
        }

        Text {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: Theme.space.m
            text: face.item.status !== "" ? face.item.status : face.item.kind
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            font.capitalization: Font.Capitalize
            color: Theme.color.inkMuted
        }

        Text {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Theme.space.m
            text: Number(face.item.words).toLocaleString(Qt.locale("en_GB"), "f", 0) + " words"
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            color: Theme.color.inkMuted
        }
    }

    // The colour of the cards, and a legend of the point of view colours.
    Flow {
        id: colourBar

        x: Theme.space.xl
        y: Theme.space.l
        width: parent.width - 2 * Theme.space.xl
        spacing: Theme.space.s

        ScopeChoice {
            wholeBook: board.wholeBook
            onChosen: whole => board.scopeChosen(whole)
        }

        Item {
            width: Theme.space.l
            height: 1
        }

        Choice {
            objectName: "cardsByLabel"
            label: "Colour by label"
            chosen: Settings.cardColour === "label"
            onClicked: Settings.cardColour = "label"
        }

        Choice {
            objectName: "cardsByPov"
            label: "Colour by POV"
            chosen: Settings.cardColour === "pov"
            onClicked: Settings.cardColour = "pov"
        }

        Repeater {
            model: Settings.cardColour === "pov" ? board.povs : []

            Row {
                id: entry

                required property string modelData

                height: 32
                spacing: Theme.space.xs
                leftPadding: Theme.space.s

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 10
                    height: 10
                    radius: 5
                    color: board.povColor(entry.modelData)
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: entry.modelData
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.small
                    color: Theme.color.inkMuted
                }
            }
        }
    }

    GridView {
        id: grid

        objectName: "corkboard"
        anchors.fill: parent
        anchors.margins: Theme.space.xl
        anchors.topMargin: colourBar.visible ? colourBar.y + colourBar.height + Theme.space.s : Theme.space.xl
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

            CardFace {
                id: card

                x: Theme.space.s
                y: Theme.space.s
                width: parent.width - 2 * Theme.space.s
                height: parent.height - 2 * Theme.space.s
                item: cell.modelData
                marked: board.dropIndex === cell.index && board.dragId !== cell.modelData.id
                opacity: board.dragId === cell.modelData.id ? 0.5 : 1

                // A click with no drag opens the item.
                TapHandler {
                    onTapped: board.opened(cell.modelData.id)
                }

                Activator {
                    name: "Open " + cell.modelData.title
                    onActivated: board.opened(cell.modelData.id)
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
