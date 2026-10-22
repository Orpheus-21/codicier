pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes

// The characters, and the other entities with a relation, on a circle, with
// a line for each relation and its name in the middle. A click on a name
// opens the entity.
Item {
    id: graph

    Accessible.role: Accessible.Pane
    Accessible.name: "Relationship graph"

    required property ProjectSession session

    // A click on an entity asks to open it.
    signal opened(id: string)

    property var nodes: []
    property var links: []
    readonly property real radius: Math.max(80, Math.min(width, height) / 2 - 90)
    readonly property point centre: Qt.point(width / 2, height / 2)

    function refresh() {
        links = session.relations("");
        const linked = new Set();
        for (const link of links) {
            linked.add(link.from);
            linked.add(link.to);
        }
        nodes = session.items().filter(item => item.kind === "character" || linked.has(item.id));
    }

    // The place of a node on the circle, by its id.
    function placeOf(id: string): point {
        const index = nodes.findIndex(node => node.id === id);
        const angle = 2 * Math.PI * index / Math.max(1, nodes.length) - Math.PI / 2;
        return Qt.point(centre.x + radius * Math.cos(angle), centre.y + radius * Math.sin(angle));
    }

    Component.onCompleted: refresh()
    onVisibleChanged: if (visible) refresh()

    Connections {
        target: graph.session

        function onEdited() {
            graph.refresh();
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.color.paper
    }

    Text {
        visible: graph.nodes.length === 0
        anchors.centerIn: parent
        text: "Add characters to the codex, and link them in the inspector."
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    Repeater {
        model: graph.links

        Item {
            id: link

            required property var modelData

            readonly property point a: graph.placeOf(modelData.from)
            readonly property point b: graph.placeOf(modelData.to)
            // The arrow points to the second entity, and stops before its name.
            readonly property real angle: Math.atan2(b.y - a.y, b.x - a.x)
            readonly property point tip: Qt.point(b.x - 22 * Math.cos(angle), b.y - 22 * Math.sin(angle))

            anchors.fill: parent

            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    strokeColor: link.modelData.parent ? Theme.color.accent : Theme.color.inkMuted
                    strokeWidth: 1.5
                    fillColor: "transparent"
                    startX: link.a.x
                    startY: link.a.y

                    PathLine {
                        x: link.tip.x
                        y: link.tip.y
                    }

                    PathLine {
                        x: link.tip.x - 9 * Math.cos(link.angle - 0.45)
                        y: link.tip.y - 9 * Math.sin(link.angle - 0.45)
                    }

                    PathMove {
                        x: link.tip.x
                        y: link.tip.y
                    }

                    PathLine {
                        x: link.tip.x - 9 * Math.cos(link.angle + 0.45)
                        y: link.tip.y - 9 * Math.sin(link.angle + 0.45)
                    }
                }
            }

            Text {
                x: (link.a.x + link.b.x) / 2 - width / 2
                y: (link.a.y + link.b.y) / 2 - height / 2
                padding: 2
                text: link.modelData.name
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.small
                color: Theme.color.inkMuted

                Rectangle {
                    anchors.fill: parent
                    z: -1
                    radius: Theme.radius.small
                    color: Theme.color.paper
                }
            }
        }
    }

    Repeater {
        model: graph.nodes

        Rectangle {
            id: node

            required property var modelData

            readonly property point place: graph.placeOf(modelData.id)

            objectName: "graphNode"
            x: place.x - width / 2
            y: place.y - height / 2
            width: Math.min(160, name.implicitWidth + 2 * Theme.space.m)
            height: 34
            radius: 17
            color: modelData.id === graph.session.currentId ? Theme.color.selection : Theme.color.surface
            border.width: 1
            border.color: Theme.color.rule
            Accessible.role: Accessible.Button
            Accessible.name: modelData.title

            Text {
                id: name

                anchors.centerIn: parent
                width: Math.min(implicitWidth, 160 - 2 * Theme.space.m)
                text: node.modelData.title
                elide: Text.ElideRight
                font.family: Theme.type.reading
                font.pixelSize: Theme.type.body + 1
                color: Theme.color.ink
            }

            TapHandler {
                onTapped: graph.opened(node.modelData.id)
            }

            Activator {
                name: "Open " + node.modelData.title
                onActivated: graph.opened(node.modelData.id)
            }
        }
    }
}
