pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Finds a command or a binder item by name. Up and Down choose, and Enter
// runs the command or opens the item.
T.Popup {
    id: finder

    required property ProjectSession session
    // The commands of the view: each has a name, a key, and a run function.
    required property var commands

    property var entries: []
    property var shown: []

    // The rank of a match: 0 for a match at the start, 1 at the start of a
    // word, 2 inside a word, 3 for the letters in order. -1 is no match.
    function rank(name: string, query: string): int {
        const text = name.toLowerCase();
        const at = text.indexOf(query);
        if (at === 0)
            return 0;
        if (at > 0)
            return /\w/.test(text[at - 1]) ? 2 : 1;
        let from = 0;
        for (const c of query.replace(/ /g, "")) {
            from = text.indexOf(c, from) + 1;
            if (from === 0)
                return -1;
        }
        return 3;
    }

    function filter() {
        const query = field.text.trim().toLowerCase();
        let list = [];
        for (let i = 0; i < entries.length; ++i) {
            const r = query === "" ? 0 : rank(entries[i].name, query);
            if (r >= 0)
                list.push({ entry: entries[i], rank: r, order: i });
        }
        // Commands come before items at the same rank. Array.sort is stable.
        list.sort((a, b) => a.rank - b.rank || a.order - b.order);
        shown = list.slice(0, 50).map(match => match.entry);
        results.currentIndex = shown.length > 0 ? 0 : -1;
    }

    function run(row: int) {
        if (row < 0 || row >= shown.length)
            return;
        const entry = shown[row];
        close();
        if (entry.command)
            entry.command.run();
        else
            session.currentId = entry.id;
    }

    function move(delta: int) {
        if (shown.length === 0)
            return;
        results.currentIndex = Math.max(0, Math.min(shown.length - 1, results.currentIndex + delta));
        results.positionViewAtIndex(results.currentIndex, ListView.Contain);
    }

    x: Math.round((parent.width - width) / 2)
    y: Math.round(parent.height * 0.12)
    width: Math.min(560, parent.width - 2 * Theme.space.l)
    height: Math.min(460, parent.height * 0.7)
    padding: Theme.space.s
    modal: true
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    onAboutToShow: {
        let list = [];
        for (const command of commands.filter(command => !command.hidden))
            list.push({ name: command.name, detail: command.sequence, command: command });
        for (const item of session.items())
            list.push({ name: item.title, detail: item.path, id: item.id });
        entries = list;
        field.text = "";
        filter();
        field.forceActiveFocus();
    }

    T.Overlay.modal: Rectangle {
        color: Qt.alpha(Theme.color.ink, 0.25)
    }

    background: Rectangle {
        radius: Theme.radius.medium
        color: Theme.color.paper
        border.width: 1
        border.color: Theme.color.rule
    }

    contentItem: Item {
        TextField {
            id: field

            objectName: "paletteField"
            width: parent.width
            iconName: "search"
            placeholderText: "Find a command or an item"
            onTextChanged: finder.filter()
            Keys.onDownPressed: finder.move(1)
            Keys.onUpPressed: finder.move(-1)
            Keys.onReturnPressed: finder.run(results.currentIndex)
            Keys.onEnterPressed: finder.run(results.currentIndex)
        }

        ListView {
            id: results

            objectName: "paletteResults"
            anchors.top: field.bottom
            anchors.topMargin: Theme.space.s
            anchors.bottom: parent.bottom
            width: parent.width
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: finder.shown

            WheelHandler {
                onWheel: event => {
                    const end = Math.max(0, results.contentHeight - results.height);
                    results.contentY = Math.min(end, Math.max(0, results.contentY - event.angleDelta.y / 2));
                }
            }

            delegate: T.ItemDelegate {
                id: row

                required property var modelData
                required property int index

                width: ListView.view.width
                implicitHeight: 34
                leftPadding: Theme.space.s
                rightPadding: Theme.space.s
                highlighted: ListView.isCurrentItem
                onClicked: finder.run(index)

                contentItem: Item {
                    Icon {
                        id: kindIcon

                        anchors.verticalCenter: parent.verticalCenter
                        name: row.modelData.command ? "chevron-right" : "file-text"
                        size: 15
                        color: Theme.color.inkMuted
                    }

                    Text {
                        id: name

                        anchors.left: kindIcon.right
                        anchors.leftMargin: Theme.space.s
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(implicitWidth, parent.width - kindIcon.width - Theme.space.s)
                        text: row.modelData.name
                        elide: Text.ElideRight
                        font.family: Theme.type.ui
                        font.pixelSize: Theme.type.body
                        color: Theme.color.ink
                    }

                    Text {
                        anchors.left: name.right
                        anchors.leftMargin: Theme.space.m
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        horizontalAlignment: Text.AlignRight
                        text: row.modelData.detail ?? ""
                        elide: Text.ElideLeft
                        font.family: Theme.type.ui
                        font.pixelSize: Theme.type.small
                        color: Theme.color.inkMuted
                    }
                }

                background: Rectangle {
                    radius: Theme.radius.small
                    color: row.highlighted || row.down ? Theme.color.selection : "transparent"
                }
            }
        }
    }
}
