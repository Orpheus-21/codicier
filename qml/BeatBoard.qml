pragma ComponentBehavior: Bound

import QtQuick

// The beats of the plot framework of the project as columns, with the
// scenes on each beat. A drag of a scene to another column puts it on that
// beat. A click on a scene opens it.
Item {
    id: board

    required property ProjectSession session

    // A click on a scene asks to open it.
    signal opened(id: string)

    property var columns: []
    property string dragId: ""
    property int dropColumn: -1

    function refresh() {
        columns = session.beatBoard();
    }

    function drop() {
        if (dragId !== "" && dropColumn >= 0)
            session.setField(dragId, "beat", columns[dropColumn].id);
        dragId = "";
        dropColumn = -1;
    }

    Component.onCompleted: refresh()
    onVisibleChanged: if (visible) refresh()

    Connections {
        target: board.session

        function onEdited() {
            board.refresh();
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.color.paper
    }

    Flow {
        id: chooser

        x: Theme.space.xl
        y: Theme.space.l
        width: parent.width - 2 * Theme.space.xl
        spacing: Theme.space.xs

        Repeater {
            model: board.session.frameworks()

            Choice {
                required property var modelData

                objectName: "framework-" + modelData.id
                label: modelData.name
                chosen: board.session.framework === modelData.id
                onClicked: board.session.setFramework(modelData.id)
            }
        }
    }

    Text {
        visible: board.columns.length === 0
        anchors.centerIn: parent
        text: "Choose a framework to place the scenes on its beats."
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    ListView {
        id: list

        objectName: "beatColumns"
        anchors.top: chooser.bottom
        anchors.topMargin: Theme.space.l
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.space.l
        x: Theme.space.xl
        width: parent.width - 2 * Theme.space.xl
        orientation: ListView.Horizontal
        spacing: Theme.space.m
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: board.columns

        WheelHandler {
            onWheel: event => {
                const end = Math.max(0, list.contentWidth - list.width);
                list.contentX = Math.min(end, Math.max(0, list.contentX - event.angleDelta.y / 2));
            }
        }

        delegate: Rectangle {
            id: column

            required property var modelData
            required property int index

            objectName: "beatColumn"
            width: 220
            height: ListView.view.height
            radius: Theme.radius.small
            color: Theme.color.surface
            border.width: board.dropColumn === index ? 2 : 1
            border.color: board.dropColumn === index ? Theme.color.accent : Theme.color.rule
            Accessible.role: Accessible.List
            Accessible.name: modelData.name

            Column {
                x: Theme.space.m
                y: Theme.space.m
                width: parent.width - 2 * Theme.space.m
                spacing: Theme.space.s

                Text {
                    width: parent.width
                    text: column.modelData.name
                    wrapMode: Text.Wrap
                    font.family: Theme.type.reading
                    font.pixelSize: Theme.type.large
                    color: Theme.color.ink
                }

                Text {
                    width: parent.width
                    text: column.modelData.description
                    wrapMode: Text.Wrap
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.small
                    color: Theme.color.inkMuted
                }

                Repeater {
                    model: column.modelData.scenes

                    Rectangle {
                        id: card

                        required property var modelData

                        objectName: "beatScene"
                        width: parent.width
                        height: 36
                        radius: Theme.radius.small
                        color: Theme.color.paper
                        border.width: 1
                        border.color: Theme.color.rule
                        opacity: board.dragId === modelData.id ? 0.5 : 1
                        Accessible.role: Accessible.ListItem
                        Accessible.name: modelData.title

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            x: Theme.space.s
                            width: parent.width - 2 * Theme.space.s
                            text: card.modelData.title
                            elide: Text.ElideRight
                            font.family: Theme.type.ui
                            font.pixelSize: Theme.type.body
                            color: Theme.color.ink
                        }

                        TapHandler {
                            onTapped: board.opened(card.modelData.id)
                        }

                        DragHandler {
                            target: null
                            onActiveChanged: {
                                if (active)
                                    board.dragId = card.modelData.id;
                                else
                                    board.drop();
                            }
                            onCentroidChanged: {
                                if (!active)
                                    return;
                                const point = card.mapToItem(list.contentItem, centroid.position.x, centroid.position.y);
                                board.dropColumn = list.indexAt(point.x, point.y);
                            }
                        }
                    }
                }
            }
        }
    }
}
