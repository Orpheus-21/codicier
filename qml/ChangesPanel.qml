pragma ComponentBehavior: Bound

import QtQuick

// The tracked changes of the open text, in the place of the binder list.
// Each change shows the removed words struck through and the added words in
// the accent colour, with Accept and Reject.
Item {
    id: panel

    required property ProjectSession session

    // Escape in the panel.
    signal closeRequested
    // A click on a change asks to select its words on the page.
    signal placed(at: int, length: int)

    property var entries: []

    function refresh() {
        entries = session.changes();
    }

    function start() {
        refresh();
        forceActiveFocus();
    }

    // The words of a change as markup: the removed ones struck through.
    function markup(change: var): string {
        const escape = text => text.trim().replace(/&/g, "&amp;").replace(/</g, "&lt;");
        let parts = [];
        if (change.removed.trim() !== "")
            parts.push("<s><font color=\"" + Theme.color.inkMuted + "\">" + escape(change.removed) + "</font></s>");
        if (change.added.trim() !== "")
            parts.push("<font color=\"" + Theme.color.accent + "\">" + escape(change.added) + "</font>");
        return parts.join("&nbsp;&nbsp;");
    }

    Keys.onEscapePressed: closeRequested()

    Connections {
        target: panel.session

        function onTrackChanged() {
            panel.refresh();
        }

        function onWordsChanged() {
            if (panel.visible)
                panel.refresh();
        }
    }

    Column {
        id: header

        width: parent.width
        spacing: Theme.space.s

        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: !panel.session.tracking ? "The changes of this text are not tracked."
                : panel.entries.length === 0 ? "No change since the tracking started."
                : panel.entries.length === 1 ? "1 change since the tracking started."
                : panel.entries.length + " changes since the tracking started."
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.inkMuted
        }

        Button {
            objectName: "startTracking"
            visible: !panel.session.tracking
            kind: "primary"
            text: "Track changes"
            onClicked: panel.session.startTracking()
        }

        Button {
            objectName: "acceptAll"
            visible: panel.session.tracking
            text: "Accept all and stop"
            onClicked: panel.session.acceptAll()
        }
    }

    ListView {
        id: list

        objectName: "changeList"
        anchors.top: header.bottom
        anchors.topMargin: Theme.space.m
        anchors.bottom: parent.bottom
        width: parent.width
        clip: true
        spacing: Theme.space.s
        boundsBehavior: Flickable.StopAtBounds
        model: panel.entries

        WheelHandler {
            onWheel: event => {
                const end = Math.max(0, list.contentHeight - list.height);
                list.contentY = Math.min(end, Math.max(0, list.contentY - event.angleDelta.y / 2));
            }
        }

        delegate: Rectangle {
            id: entry

            required property var modelData
            required property int index

            width: ListView.view.width
            height: entryColumn.implicitHeight + 2 * Theme.space.s
            radius: Theme.radius.small
            color: Theme.color.surface
            Accessible.role: Accessible.ListItem
            Accessible.name: "Removed: " + modelData.removed + ". Added: " + modelData.added

            TapHandler {
                onTapped: panel.placed(entry.modelData.at, entry.modelData.added.length)
            }

            Column {
                id: entryColumn

                x: Theme.space.s
                y: Theme.space.s
                width: parent.width - 2 * Theme.space.s
                spacing: Theme.space.xs

                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    textFormat: Text.StyledText
                    text: panel.markup(entry.modelData)
                    font.family: Theme.type.reading
                    font.pixelSize: Theme.type.body
                    color: Theme.color.ink
                }

                Row {
                    spacing: Theme.space.xs

                    Button {
                        text: "Accept"
                        onClicked: panel.session.acceptChange(entry.index)
                    }

                    Button {
                        text: "Reject"
                        onClicked: panel.session.rejectChange(entry.index)
                    }
                }
            }
        }
    }
}
