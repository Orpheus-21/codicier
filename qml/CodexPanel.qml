pragma ComponentBehavior: Bound

import QtQuick

// The codex: the entities of the project, grouped by type, with a filter.
// A click or Enter opens an entity for edit.
Item {
    id: panel

    required property ProjectSession session

    // A click or Enter on an entity.
    signal opened(id: string)
    // Escape in the panel.
    signal closeRequested

    // The types in the order of the list, with the heading of each.
    readonly property var types: [
        ["character", "Characters"], ["place", "Places"], ["item", "Items"],
        ["faction", "Factions"], ["lore", "Lore"], ["term", "Terms"]
    ]
    property var entities: []

    function heading(kind: string): string {
        const type = types.find(entry => entry[0] === kind);
        return type ? type[1] : kind;
    }

    function refresh() {
        const query = field.text.trim().toLowerCase();
        const order = kind => types.findIndex(entry => entry[0] === kind);
        entities = session.items()
            .filter(item => order(item.kind) >= 0 && item.title.toLowerCase().includes(query))
            .sort((a, b) => order(a.kind) - order(b.kind) || a.title.localeCompare(b.title));
        // The open entity is the current row, else the first row.
        list.currentIndex = Math.max(entities.findIndex(item => item.id === session.currentId),
                                     entities.length > 0 ? 0 : -1);
    }

    function start() {
        refresh();
        field.forceActiveFocus();
        field.selectAll();
    }

    function openRow(row: int) {
        if (row >= 0 && row < entities.length)
            opened(entities[row].id);
    }

    Keys.onEscapePressed: closeRequested()

    // The list follows the binder while the panel is open.
    Connections {
        target: panel.session

        function onEdited() {
            if (panel.visible)
                panel.refresh();
        }
    }

    TextField {
        id: field

        objectName: "codexField"
        width: parent.width
        iconName: "search"
        placeholderText: "Find in the codex"
        onTextChanged: panel.refresh()
        Keys.onReturnPressed: panel.openRow(list.currentIndex)
        Keys.onEnterPressed: panel.openRow(list.currentIndex)
        Keys.onDownPressed: list.currentIndex = Math.min(list.currentIndex + 1, panel.entities.length - 1)
        Keys.onUpPressed: list.currentIndex = Math.max(list.currentIndex - 1, 0)
    }

    Text {
        visible: panel.entities.length === 0
        anchors.top: field.bottom
        anchors.topMargin: Theme.space.l
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: field.text.trim() === "" ? "The codex is empty. Add a character or a place with the + button."
                                        : "No match"
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    ListView {
        id: list

        objectName: "codexList"
        anchors.top: field.bottom
        anchors.topMargin: Theme.space.s
        anchors.bottom: parent.bottom
        width: parent.width
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: panel.entities
        section.property: "kind"
        section.delegate: Text {
            required property string section

            width: ListView.view.width
            topPadding: Theme.space.m
            bottomPadding: Theme.space.xs
            leftPadding: Theme.space.s
            text: panel.heading(section)
            Accessible.role: Accessible.Heading
            Accessible.name: text
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            font.weight: Font.DemiBold
            font.letterSpacing: 1.2
            font.capitalization: Font.AllUppercase
            color: Theme.color.inkMuted
        }

        WheelHandler {
            onWheel: event => {
                const end = Math.max(0, list.contentHeight - list.height);
                list.contentY = Math.min(end, Math.max(0, list.contentY - event.angleDelta.y / 2));
            }
        }

        delegate: ListItem {
            required property var modelData
            required property int index

            width: ListView.view.width
            text: modelData.title
            highlighted: ListView.isCurrentItem || modelData.id === panel.session.currentId
            onClicked: panel.openRow(index)
        }
    }
}
