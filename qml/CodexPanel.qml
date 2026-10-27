pragma ComponentBehavior: Bound

import QtQml
import QtQuick
import QtQuick.Templates as T

// The codex: the entities of the project, grouped by type, with a filter.
// A click or Enter opens an entity for edit.
Item {
    id: panel

    Accessible.role: Accessible.Pane
    Accessible.name: "Codex"

    required property ProjectSession session

    // A click or Enter on an entity.
    signal opened(id: string)
    // Escape in the panel.
    signal closeRequested
    // The author wants a new entity of a kind.
    signal addRequested(kind: string)

    // The entity types in the order of the list: kind, name, and fields.
    property var types: []
    property var entities: []
    // Names that come back in the text and have no entry: name and count.
    property var suggestions: []
    // The title of the first scene in which each entity appears, by id.
    property var firsts: ({})

    // Adds the name as a new character. The codex lists it at once.
    function addSuggestion(name: string) {
        const id = session.add("character");
        if (id === "")
            return;
        session.rename(id, name);
        suggestions = session.suggestedEntries();
        firsts = session.firstAppearances();
    }

    function heading(kind: string): string {
        const type = types.find(entry => entry.kind === kind);
        return type ? type.name : kind;
    }

    function refresh() {
        types = session.entityTypes();
        const query = field.text.trim().toLowerCase();
        const order = kind => types.findIndex(entry => entry.kind === kind);
        entities = session.items()
            .filter(item => order(item.kind) >= 0 && item.title.toLowerCase().includes(query))
            .sort((a, b) => order(a.kind) - order(b.kind) || a.title.localeCompare(b.title));
        // The open entity is the current row, else the first row.
        list.currentIndex = Math.max(entities.findIndex(item => item.id === session.currentId),
                                     entities.length > 0 ? 0 : -1);
    }

    function editTypes() {
        typeDialog.open();
    }

    function start() {
        refresh();
        suggestions = session.suggestedEntries();
        firsts = session.firstAppearances();
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
        width: parent.width - addButton.width - Theme.space.xs
        iconName: "search"
        placeholderText: "Find in the codex"
        onTextChanged: panel.refresh()
        Keys.onReturnPressed: panel.openRow(list.currentIndex)
        Keys.onEnterPressed: panel.openRow(list.currentIndex)
        Keys.onDownPressed: list.currentIndex = Math.min(list.currentIndex + 1, panel.entities.length - 1)
        Keys.onUpPressed: list.currentIndex = Math.max(list.currentIndex - 1, 0)
    }

    IconButton {
        id: addButton

        objectName: "codexAdd"
        anchors.right: parent.right
        anchors.verticalCenter: field.verticalCenter
        iconName: "plus"
        label: "Add to the codex"
        onClicked: addMenu.popup(addButton, 0, addButton.height)
    }

    Menu {
        id: addMenu

        Instantiator {
            model: panel.types

            delegate: T.Action {
                required property var modelData

                text: "Add to " + modelData.name
                onTriggered: panel.addRequested(modelData.kind)
            }

            onObjectAdded: (index, action) => addMenu.insertAction(index, action as T.Action)
            onObjectRemoved: (index, action) => addMenu.removeAction(action as T.Action)
        }

        T.Action {
            text: "Types and fields"
            onTriggered: panel.editTypes()
        }
    }

    TypeDialog {
        id: typeDialog

        parent: T.Overlay.overlay
        anchors.centerIn: parent
        session: panel.session
        onClosed: panel.refresh()
    }

    Text {
        visible: panel.entities.length === 0
        anchors.top: field.bottom
        anchors.topMargin: Theme.space.l
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: field.text.trim() === "" ? "The codex is empty. Add an entity with the + button."
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
        anchors.bottom: suggestionBox.visible ? suggestionBox.top : parent.bottom
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
            trailing: panel.firsts[modelData.id] ? "First in " + panel.firsts[modelData.id] : ""
            highlighted: ListView.isCurrentItem || modelData.id === panel.session.currentId
            onClicked: panel.openRow(index)
        }
    }

    // Names to add: a name that comes back in the text and has no entry.
    Column {
        id: suggestionBox

        objectName: "codexSuggestions"
        visible: panel.suggestions.length > 0
        anchors.bottom: parent.bottom
        width: parent.width
        spacing: Theme.space.xs

        Text {
            leftPadding: Theme.space.s
            text: "Names to add as characters"
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            font.weight: Font.DemiBold
            font.letterSpacing: 1.2
            font.capitalization: Font.AllUppercase
            color: Theme.color.inkMuted
        }

        Flow {
            width: parent.width
            spacing: Theme.space.xs

            Repeater {
                model: panel.suggestions

                Button {
                    required property var modelData

                    objectName: "codexSuggestion"
                    text: modelData.name + " (" + modelData.count + ")"
                    onClicked: panel.addSuggestion(modelData.name)
                }
            }
        }
    }
}
