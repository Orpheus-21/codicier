pragma ComponentBehavior: Bound

import QtQuick

// The items with a date in story time, on one track for each POV character
// or for each plotline. The place of a card shows its date. A click on a
// card opens the item.
Item {
    id: timeline

    Accessible.role: Accessible.Pane
    Accessible.name: "Timeline"

    required property ProjectSession session

    // A click on a card asks to open the item.
    signal opened(id: string)
    // The author wants to change the calendar.
    signal calendarRequested

    property string groupBy: "pov"
    property var tracks: []
    // The first and the last day of all events.
    property real first: 0
    property real last: 0
    readonly property int nameWidth: 160
    readonly property int cardWidth: 150
    readonly property int trackHeight: 96
    // At least one card width between the first and the last event, so the
    // cards spread over the width.
    readonly property real span: Math.max(1, last - first)
    readonly property real usable: Math.max(flick.width - nameWidth - cardWidth,
                                            (tracks.reduce((n, track) => n + track.events.length, 0)) * 60)
    // A track has a row of cards above its line and one or more rows below.
    readonly property real rowHeight: trackHeight / 2
    // For each track, the row of each event: the first row that is free at
    // the x of the event. Cards close in time go to the next row.
    readonly property var rows: tracks.map(track => {
        let ends = [];
        return track.events.map(event => {
            const x = xOf(event.day);
            let row = ends.findIndex(end => end <= x);
            if (row < 0) {
                row = ends.length;
                ends.push(0);
            }
            ends[row] = x + cardWidth;
            return row;
        });
    })
    // The height and the top of each track.
    readonly property var heights: rows.map(list => Math.max(2, ...list.map(row => row + 1)) * rowHeight)
    readonly property var tops: heights.map((height, i) => heights.slice(0, i).reduce((sum, h) => sum + h, 0))

    function xOf(day: real): real {
        return nameWidth + (day - first) / span * usable;
    }

    function refresh() {
        tracks = session.timeline(groupBy);
        // The QML engine has no flatMap.
        let days = [];
        for (const track of tracks)
            days = days.concat(track.events.map(event => event.day));
        first = days.length > 0 ? Math.min(...days) : 0;
        last = days.length > 0 ? Math.max(...days) : 0;
    }

    onGroupByChanged: refresh()
    Component.onCompleted: refresh()
    onVisibleChanged: if (visible) refresh()

    Connections {
        target: timeline.session

        function onEdited() {
            timeline.refresh();
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.color.paper
    }

    Row {
        id: chooser

        x: Theme.space.xl
        y: Theme.space.l
        spacing: Theme.space.xs

        Choice {
            objectName: "byPov"
            label: "By POV"
            chosen: timeline.groupBy === "pov"
            onClicked: timeline.groupBy = "pov"
        }

        Choice {
            objectName: "byPlotline"
            label: "By plotline"
            chosen: timeline.groupBy === "plotline"
            onClicked: timeline.groupBy = "plotline"
        }

        Button {
            objectName: "calendarButton"
            text: "Calendar"
            iconName: "calendar"
            onClicked: timeline.calendarRequested()
        }
    }

    Text {
        visible: timeline.tracks.length === 0
        anchors.centerIn: parent
        text: "Give a scene or an entity a date in the story to see it here."
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    Flickable {
        id: flick

        objectName: "timelineTracks"
        anchors.top: chooser.bottom
        anchors.topMargin: Theme.space.l
        anchors.bottom: parent.bottom
        x: Theme.space.xl
        width: parent.width - 2 * Theme.space.xl
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: timeline.nameWidth + timeline.usable + timeline.cardWidth
        contentHeight: timeline.heights.reduce((sum, height) => sum + height, 0)

        WheelHandler {
            onWheel: event => {
                const end = Math.max(0, flick.contentWidth - flick.width);
                flick.contentX = Math.min(end, Math.max(0, flick.contentX - event.angleDelta.y / 2));
            }
        }

        Repeater {
            model: timeline.tracks

            Item {
                id: track

                required property var modelData
                required property int index

                objectName: "timelineTrack"
                y: timeline.tops[index] ?? 0
                width: flick.contentWidth
                height: timeline.heights[index] ?? timeline.trackHeight

                Rectangle {
                    y: timeline.rowHeight
                    x: timeline.nameWidth
                    width: parent.width - timeline.nameWidth
                    height: 1
                    color: Theme.color.rule
                }

                Text {
                    width: timeline.nameWidth - Theme.space.s
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    text: track.modelData.name
                    elide: Text.ElideRight
                    font.family: Theme.type.reading
                    font.pixelSize: Theme.type.body + 1
                    color: Theme.color.ink
                }

                Repeater {
                    model: track.modelData.events

                    Rectangle {
                        id: card

                        required property var modelData
                        required property int index

                        objectName: "timelineEvent"
                        readonly property int row: timeline.rows[track.index]?.[index] ?? 0

                        x: timeline.xOf(modelData.day)
                        y: row * timeline.rowHeight + (row === 0 ? 6 : 4)
                        width: timeline.cardWidth - Theme.space.s
                        height: timeline.rowHeight - 10
                        radius: Theme.radius.small
                        color: Theme.color.surface
                        border.width: 1
                        border.color: modelData.id === timeline.session.currentId ? Theme.color.accent : Theme.color.rule
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.title + ", " + modelData.label

                        Column {
                            x: Theme.space.s
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 2 * Theme.space.s

                            Text {
                                width: parent.width
                                text: card.modelData.title
                                elide: Text.ElideRight
                                font.family: Theme.type.ui
                                font.pixelSize: Theme.type.small + 1
                                color: Theme.color.ink
                            }

                            Text {
                                width: parent.width
                                text: card.modelData.label
                                elide: Text.ElideRight
                                font.family: Theme.type.ui
                                font.pixelSize: Theme.type.small
                                color: Theme.color.inkMuted
                            }
                        }

                        TapHandler {
                            onTapped: timeline.opened(card.modelData.id)
                        }

                        Activator {
                            name: "Open " + card.modelData.title
                            onActivated: timeline.opened(card.modelData.id)
                        }
                    }
                }
            }
        }
    }
}
