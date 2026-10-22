pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes

// The parent links as a family tree: a row for each generation, oldest at
// the top, with a line from each parent down to each child. A click on a
// person opens the entity.
Item {
    id: tree

    required property ProjectSession session

    // A click on a person asks to open the entity.
    signal opened(id: string)

    property var generations: []
    property var links: []
    readonly property int rowHeight: 120
    readonly property int nodeWidth: 150
    readonly property int nodeHeight: 36

    function refresh() {
        const data = session.familyTree();
        generations = data.generations;
        links = data.links;
    }

    // The centre of a person in the tree, by id.
    function placeOf(id: string): point {
        for (let row = 0; row < generations.length; ++row) {
            const index = generations[row].findIndex(person => person.id === id);
            if (index >= 0) {
                const step = width / (generations[row].length + 1);
                return Qt.point(step * (index + 1), Theme.space.xxl + row * rowHeight + nodeHeight / 2);
            }
        }
        return Qt.point(0, 0);
    }

    Component.onCompleted: refresh()
    onVisibleChanged: if (visible) refresh()

    Connections {
        target: tree.session

        function onEdited() {
            tree.refresh();
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.color.paper
    }

    Text {
        visible: tree.generations.length === 0
        anchors.centerIn: parent
        text: "Link a parent to a child with \"Parent of\" in the inspector of a character."
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    Repeater {
        model: tree.links

        Shape {
            id: line

            required property var modelData

            readonly property point a: tree.placeOf(modelData.from)
            readonly property point b: tree.placeOf(modelData.to)

            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            // Down from the parent, across, and down to the child.
            ShapePath {
                strokeColor: Theme.color.inkMuted
                strokeWidth: 1.5
                fillColor: "transparent"
                startX: line.a.x
                startY: line.a.y + tree.nodeHeight / 2

                PathLine {
                    x: line.a.x
                    y: (line.a.y + line.b.y) / 2
                }

                PathLine {
                    x: line.b.x
                    y: (line.a.y + line.b.y) / 2
                }

                PathLine {
                    x: line.b.x
                    y: line.b.y - tree.nodeHeight / 2
                }
            }
        }
    }

    Repeater {
        model: tree.generations

        Repeater {
            id: generation

            required property var modelData

            model: modelData

            Rectangle {
                id: person

                required property var modelData

                readonly property point place: tree.placeOf(modelData.id)

                objectName: "treePerson"
                x: place.x - width / 2
                y: place.y - height / 2
                width: tree.nodeWidth
                height: tree.nodeHeight
                radius: Theme.radius.small
                color: modelData.id === tree.session.currentId ? Theme.color.selection : Theme.color.surface
                border.width: 1
                border.color: Theme.color.rule
                Accessible.role: Accessible.Button
                Accessible.name: modelData.title

                Text {
                    anchors.centerIn: parent
                    width: parent.width - 2 * Theme.space.s
                    horizontalAlignment: Text.AlignHCenter
                    text: person.modelData.title
                    elide: Text.ElideRight
                    font.family: Theme.type.reading
                    font.pixelSize: Theme.type.body + 1
                    color: Theme.color.ink
                }

                TapHandler {
                    onTapped: tree.opened(person.modelData.id)
                }

                Activator {
                    name: "Open " + person.modelData.title
                    onActivated: tree.opened(person.modelData.id)
                }
            }
        }
    }
}
