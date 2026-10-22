pragma ComponentBehavior: Bound

import QtQuick

// The scenes of the manuscript against the plotlines. A filled cell puts the
// scene on the plotline. A click switches a cell, and a drag of a mark to
// another row moves the scene from one plotline to the other.
Item {
    id: grid

    Accessible.role: Accessible.Pane
    Accessible.name: "Plot grid"

    required property ProjectSession session

    property var scenes: []
    property var lines: []
    // The mark that the drag holds, and the row under the pointer.
    property string dragScene: ""
    property string dragLine: ""
    property int dropRow: -1
    readonly property int nameWidth: 180
    readonly property int cellWidth: 128
    readonly property int cellHeight: 40

    function refresh() {
        scenes = session.plotGrid();
        lines = session.plotlines();
    }

    // Puts a scene on a plotline, or takes it off.
    function toggle(scene: var, line: string) {
        const now = scene.plotlines;
        session.setField(scene.id, "plotlines", now.includes(line) ? now.filter(id => id !== line) : now.concat([line]));
    }

    function drop() {
        const scene = scenes.find(each => each.id === dragScene);
        if (scene && dropRow >= 0 && lines[dropRow].id !== dragLine) {
            const target = lines[dropRow].id;
            const next = scene.plotlines.filter(id => id !== dragLine);
            if (!next.includes(target))
                next.push(target);
            session.setField(scene.id, "plotlines", next);
        }
        dragScene = "";
        dragLine = "";
        dropRow = -1;
    }

    Component.onCompleted: refresh()
    onVisibleChanged: if (visible) refresh()

    Connections {
        target: grid.session

        function onEdited() {
            grid.refresh();
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.color.paper
    }

    Text {
        visible: grid.lines.length === 0 || grid.scenes.length === 0
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.space.xxl
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: "Add a plotline in the inspector of a scene to see the plot grid."
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    Flickable {
        id: flick

        objectName: "plotGrid"
        visible: grid.lines.length > 0 && grid.scenes.length > 0
        anchors.fill: parent
        anchors.margins: Theme.space.xl
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: grid.nameWidth + grid.scenes.length * grid.cellWidth
        contentHeight: (grid.lines.length + 1) * grid.cellHeight

        WheelHandler {
            onWheel: event => {
                const end = Math.max(0, flick.contentWidth - flick.width);
                flick.contentX = Math.min(end, Math.max(0, flick.contentX - event.angleDelta.y / 2));
            }
        }

        // The titles of the scenes.
        Row {
            x: grid.nameWidth

            Repeater {
                model: grid.scenes

                Text {
                    required property var modelData

                    width: grid.cellWidth
                    height: grid.cellHeight
                    leftPadding: Theme.space.s
                    rightPadding: Theme.space.s
                    verticalAlignment: Text.AlignVCenter
                    text: modelData.title
                    elide: Text.ElideRight
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.small
                    font.weight: Font.DemiBold
                    color: Theme.color.inkMuted
                }
            }
        }

        // One row for each plotline.
        Column {
            y: grid.cellHeight

            Repeater {
                model: grid.lines

                Row {
                    id: lineRow

                    required property var modelData
                    required property int index

                    objectName: "plotRow"

                    Rectangle {
                        width: grid.nameWidth
                        height: grid.cellHeight
                        color: grid.dropRow === lineRow.index ? Theme.color.selection : "transparent"

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            x: Theme.space.s
                            width: parent.width - 2 * Theme.space.s
                            text: lineRow.modelData.name
                            elide: Text.ElideRight
                            font.family: Theme.type.reading
                            font.pixelSize: Theme.type.body + 1
                            color: Theme.color.ink
                        }
                    }

                    Repeater {
                        model: grid.scenes

                        Rectangle {
                            id: cell

                            required property var modelData

                            readonly property bool on: modelData.plotlines.includes(lineRow.modelData.id)

                            objectName: "plotCell"
                            width: grid.cellWidth
                            height: grid.cellHeight
                            color: grid.dropRow === lineRow.index ? Theme.color.selection : "transparent"
                            border.width: 1
                            border.color: Theme.color.rule
                            Accessible.role: Accessible.CheckBox
                            Accessible.name: modelData.title + " on " + lineRow.modelData.name
                            Accessible.checked: on

                            Rectangle {
                                visible: cell.on
                                anchors.centerIn: parent
                                width: parent.width - 2 * Theme.space.s
                                height: 14
                                radius: 7
                                color: Theme.color.accent
                                opacity: grid.dragScene === cell.modelData.id && grid.dragLine === lineRow.modelData.id ? 0.4 : 1
                            }

                            TapHandler {
                                onTapped: grid.toggle(cell.modelData, lineRow.modelData.id)
                            }

                            Activator {
                                onActivated: grid.toggle(cell.modelData, lineRow.modelData.id)
                            }

                            DragHandler {
                                target: null
                                enabled: cell.on
                                onActiveChanged: {
                                    if (active) {
                                        grid.dragScene = cell.modelData.id;
                                        grid.dragLine = lineRow.modelData.id;
                                    } else {
                                        grid.drop();
                                    }
                                }
                                onCentroidChanged: {
                                    if (!active)
                                        return;
                                    const y = cell.mapToItem(flick.contentItem, centroid.position.x, centroid.position.y).y;
                                    const row = Math.floor(y / grid.cellHeight) - 1;
                                    grid.dropRow = row >= 0 && row < grid.lines.length ? row : -1;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
