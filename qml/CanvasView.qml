pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import QtQuick.Templates as T

// An idea canvas: notes and cards anywhere on a free surface, with lines
// between them. Double-click an empty spot for a new note. Drag a note by
// its top strip, and a card anywhere. Drag from the dot on the right of a
// node to another node for a line. Delete removes the chosen node. Each
// change saves a moment later.
Item {
    id: canvas

    Accessible.role: Accessible.Pane
    Accessible.name: "Idea canvas"

    required property ProjectSession session
    // The canvas item, or "" when the view is hidden.
    required property string itemId

    // A double-click on a card asks to open its item.
    signal opened(id: string)

    property var nodes: []
    property var edges: []
    property string chosenId: ""
    // The node that a line starts from while the author drags it, and the
    // end of the line in canvas coordinates.
    property string linkFrom: ""
    property point linkEnd: Qt.point(0, 0)
    readonly property int nodeWidth: 220

    function load() {
        const data = itemId !== "" ? session.canvasOf(itemId) : { nodes: [], edges: [] };
        nodes = data.nodes;
        edges = data.edges;
        chosenId = "";
    }

    function changed() {
        nodes = nodes.slice();
        edges = edges.slice();
        saveTimer.restart();
    }

    function newId(): string {
        let id = "";
        do
            id = "n" + Math.floor(Math.random() * 1e9).toString(36);
        while (nodes.some(node => node.id === id));
        return id;
    }

    function addNode(x: real, y: real, text: string, item: string): string {
        const id = newId();
        nodes.push({ id: id, x: Math.max(0, Math.round(x)), y: Math.max(0, Math.round(y)), text: text, item: item });
        chosenId = id;
        changed();
        return id;
    }

    function nodeAt(id: string): var {
        return nodes.find(node => node.id === id);
    }

    function moveNode(id: string, x: real, y: real) {
        const node = nodeAt(id);
        if (!node)
            return;
        node.x = Math.max(0, Math.round(x));
        node.y = Math.max(0, Math.round(y));
        changed();
    }

    function setText(id: string, text: string) {
        const node = nodeAt(id);
        if (node && node.text !== text) {
            node.text = text;
            saveTimer.restart();
        }
    }

    function link(from: string, to: string) {
        if (from === to || from === "" || to === ""
                || edges.some(edge => (edge.from === from && edge.to === to) || (edge.from === to && edge.to === from)))
            return;
        edges.push({ from: from, to: to });
        changed();
    }

    function unlink(id: string) {
        edges = edges.filter(edge => edge.from !== id && edge.to !== id);
        changed();
    }

    function remove(id: string) {
        nodes = nodes.filter(node => node.id !== id);
        edges = edges.filter(edge => edge.from !== id && edge.to !== id);
        if (chosenId === id)
            chosenId = "";
        changed();
    }

    // The node under a point of the canvas, or "".
    function idAt(x: real, y: real): string {
        for (let at = surface.children.length - 1; at >= 0; --at) {
            const child = surface.children[at];
            if (child.objectName === "canvasNode" && x >= child.x && x < child.x + child.width
                    && y >= child.y && y < child.y + child.height)
                return child.nodeId;
        }
        return "";
    }

    onItemIdChanged: load()
    Component.onCompleted: load()
    Component.onDestruction: if (saveTimer.running) session.saveCanvas(itemId, { nodes: nodes, edges: edges })
    Keys.onDeletePressed: if (chosenId !== "") remove(chosenId)

    Timer {
        id: saveTimer

        interval: 400
        onTriggered: canvas.session.saveCanvas(canvas.itemId, { nodes: canvas.nodes, edges: canvas.edges })
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.color.paper
    }

    Row {
        id: toolbar

        x: Theme.space.xl
        y: Theme.space.m
        z: 2
        spacing: Theme.space.s

        Button {
            objectName: "addNote"
            iconName: "plus"
            text: "Note"
            onClicked: canvas.addNode(flick.contentX + Theme.space.xl, flick.contentY + Theme.space.xxl, "", "")
        }

        Button {
            objectName: "addCard"
            iconName: "plus"
            text: "Card"
            onClicked: cardPicker.popup(this, 0, height)
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: canvas.nodes.length === 0
            text: "Double-click anywhere for a note."
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.inkMuted
        }
    }

    // The texts of the manuscript, for a card.
    Menu {
        id: cardPicker

        Instantiator {
            model: cardPicker.opened ? canvas.session.items().filter(item => item.kind === "scene" || item.kind === "chapter") : []

            delegate: T.Action {
                required property var modelData

                text: modelData.title
                onTriggered: canvas.addNode(flick.contentX + Theme.space.xl, flick.contentY + Theme.space.xxl, "",
                                            modelData.id)
            }

            onObjectAdded: (index, action) => cardPicker.insertAction(index, action as T.Action)
            onObjectRemoved: (index, action) => cardPicker.removeAction(action as T.Action)
        }
    }

    Flickable {
        id: flick

        objectName: "canvasSurface"
        anchors.fill: parent
        contentWidth: Math.max(width, ...canvas.nodes.map(node => node.x + canvas.nodeWidth + 200))
        contentHeight: Math.max(height, ...canvas.nodes.map(node => node.y + 400))
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        WheelHandler {
            onWheel: event => {
                const end = Math.max(0, flick.contentHeight - flick.height);
                flick.contentY = Math.min(end, Math.max(0, flick.contentY - event.angleDelta.y / 2));
            }
        }

        Item {
            id: surface

            width: flick.contentWidth
            height: flick.contentHeight

            TapHandler {
                // The time and the place of the last tap, to find a double-click.
                property double lastTime: 0
                property point lastPlace: Qt.point(-100, -100)

                onTapped: eventPoint => {
                    canvas.chosenId = "";
                    canvas.forceActiveFocus();
                    const now = Date.now();
                    const near = Math.abs(eventPoint.position.x - lastPlace.x) < 8
                        && Math.abs(eventPoint.position.y - lastPlace.y) < 8;
                    if (near && now - lastTime < Application.styleHints.mouseDoubleClickInterval) {
                        canvas.addNode(eventPoint.position.x - 20, eventPoint.position.y - 12, "", "");
                        lastTime = 0;
                    } else {
                        lastTime = now;
                        lastPlace = eventPoint.position;
                    }
                }
            }

            // The lines, from the middle of one node to the middle of the other.
            Repeater {
                model: canvas.edges

                Shape {
                    id: line

                    required property var modelData
                    readonly property var from: canvas.nodeAt(modelData.from)
                    readonly property var to: canvas.nodeAt(modelData.to)

                    visible: from !== undefined && to !== undefined
                    preferredRendererType: Shape.CurveRenderer

                    ShapePath {
                        strokeColor: Theme.color.inkMuted
                        strokeWidth: 1.5
                        fillColor: "transparent"
                        startX: line.from ? line.from.x + canvas.nodeWidth / 2 : 0
                        startY: line.from ? line.from.y + 40 : 0

                        PathLine {
                            x: line.to ? line.to.x + canvas.nodeWidth / 2 : 0
                            y: line.to ? line.to.y + 40 : 0
                        }
                    }
                }
            }

            // The line that the author drags from a node.
            Shape {
                visible: canvas.linkFrom !== ""
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    readonly property var from: canvas.nodeAt(canvas.linkFrom)

                    strokeColor: Theme.color.accent
                    strokeWidth: 1.5
                    strokeStyle: ShapePath.DashLine
                    fillColor: "transparent"
                    startX: from ? from.x + canvas.nodeWidth : 0
                    startY: from ? from.y + 40 : 0

                    PathLine {
                        x: canvas.linkEnd.x
                        y: canvas.linkEnd.y
                    }
                }
            }

            Repeater {
                model: canvas.nodes

                Rectangle {
                    id: node

                    required property var modelData
                    readonly property string nodeId: modelData.id
                    readonly property bool card: modelData.item !== ""
                    readonly property var fields: card ? canvas.session.fields(modelData.item) : ({})
                    // The move of the drag that runs.
                    property point moving: Qt.point(0, 0)

                    objectName: "canvasNode"
                    x: modelData.x + moving.x
                    y: modelData.y + moving.y
                    z: grip.active || move.active ? 10 : 1
                    width: canvas.nodeWidth
                    height: content.implicitHeight + 2 * Theme.space.m
                    radius: Theme.radius.small
                    // Solid, so the lines end at the edge of the node.
                    color: card ? Theme.color.surface : Qt.tint(Theme.color.surface, Qt.alpha(Theme.color.accent, 0.12))
                    border.width: canvas.chosenId === nodeId ? 2 : 1
                    border.color: canvas.chosenId === nodeId ? Theme.color.accent : Theme.color.rule
                    Accessible.role: Accessible.ListItem
                    Accessible.name: card ? "Card " + (fields.title ?? "") : "Note " + modelData.text

                    function dropped() {
                        canvas.moveNode(nodeId, x, y);
                        moving = Qt.point(0, 0);
                    }

                    TapHandler {
                        property double lastTime: 0

                        onTapped: {
                            canvas.chosenId = node.nodeId;
                            canvas.forceActiveFocus();
                            // A double-click on a card opens its text.
                            const now = Date.now();
                            if (node.card && now - lastTime < Application.styleHints.mouseDoubleClickInterval)
                                canvas.opened(node.modelData.item);
                            lastTime = now;
                        }
                    }

                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: eventPoint => {
                            canvas.chosenId = node.nodeId;
                            nodeMenu.popup(node, eventPoint.position.x, eventPoint.position.y);
                        }
                    }

                    // A card moves from anywhere on it.
                    DragHandler {
                        id: move

                        enabled: node.card
                        target: null
                        onActiveTranslationChanged: if (active) node.moving = activeTranslation
                        onActiveChanged: if (!active) node.dropped()
                    }

                    Column {
                        id: content

                        x: Theme.space.m
                        y: Theme.space.m
                        width: parent.width - 2 * Theme.space.m
                        spacing: Theme.space.xs

                        // The strip that moves a note.
                        Rectangle {
                            visible: !node.card
                            width: 28
                            height: 4
                            radius: 2
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: Qt.alpha(Theme.color.inkMuted, 0.6)

                            DragHandler {
                                id: grip

                                target: null
                                margin: 10
                                onActiveTranslationChanged: if (active) node.moving = activeTranslation
                                onActiveChanged: if (!active) node.dropped()
                            }
                        }

                        Text {
                            visible: node.card
                            width: parent.width
                            text: node.fields.title ?? "A text that is gone"
                            elide: Text.ElideRight
                            font.family: Theme.type.reading
                            font.pixelSize: Theme.type.large
                            color: Theme.color.ink
                        }

                        Text {
                            visible: node.card
                            width: parent.width
                            text: (node.fields.synopsis ?? "") !== "" ? node.fields.synopsis : "No synopsis"
                            maximumLineCount: 4
                            wrapMode: Text.Wrap
                            elide: Text.ElideRight
                            font.family: Theme.type.reading
                            font.italic: true
                            font.pixelSize: Theme.type.body
                            color: Theme.color.inkMuted
                        }

                        TextEdit {
                            objectName: "noteText"
                            visible: !node.card
                            width: parent.width
                            text: node.modelData.text
                            wrapMode: TextEdit.Wrap
                            font.family: Theme.type.ui
                            font.pixelSize: Theme.type.body
                            color: Theme.color.ink
                            selectionColor: Theme.color.selection
                            selectedTextColor: Theme.color.ink
                            Accessible.name: "Text of the note"
                            Component.onCompleted: {
                                if (canvas.chosenId === node.nodeId && text === "")
                                    forceActiveFocus();
                            }
                            onTextChanged: canvas.setText(node.nodeId, text)
                        }
                    }

                    // The dot that starts a line to another node.
                    Rectangle {
                        objectName: "linkDot"
                        x: parent.width - width / 2
                        y: 40 - height / 2
                        width: 12
                        height: 12
                        radius: 6
                        color: Theme.color.paper
                        border.width: 1.5
                        border.color: Theme.color.accent
                        Accessible.role: Accessible.Button
                        Accessible.name: "Draw a line from this node"

                        DragHandler {
                            target: null
                            onActiveChanged: {
                                if (active) {
                                    canvas.linkFrom = node.nodeId;
                                } else {
                                    canvas.link(canvas.linkFrom, canvas.idAt(canvas.linkEnd.x, canvas.linkEnd.y));
                                    canvas.linkFrom = "";
                                }
                            }
                            onCentroidChanged: {
                                if (active)
                                    canvas.linkEnd = parent.mapToItem(surface, centroid.position.x, centroid.position.y);
                            }
                        }
                    }
                }
            }
        }
    }

    Menu {
        id: nodeMenu

        T.Action {
            text: "Open"
            enabled: (canvas.nodeAt(canvas.chosenId)?.item ?? "") !== ""
            onTriggered: canvas.opened(canvas.nodeAt(canvas.chosenId).item)
        }
        T.Action {
            text: "Remove its lines"
            onTriggered: canvas.unlink(canvas.chosenId)
        }
        T.Action {
            text: "Delete"
            onTriggered: canvas.remove(canvas.chosenId)
        }
    }
}
