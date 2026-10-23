pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// The quote bank: quotes from the sources, each with its source and page.
// A drag puts a quote on the page, and Return or a double click puts it at
// the cursor, with its citation, such as “Salt.” [@kurlansky2002, p. 12].
Item {
    id: panel

    Accessible.role: Accessible.Pane
    Accessible.name: "Quotes"

    required property ProjectSession session

    // Escape in the panel.
    signal closeRequested
    // The author puts a quote at the cursor of the page.
    signal inserted(text: string)

    property var entries: []
    property var sources: []
    // The key of the source for the next quote.
    property string source: ""
    property string error: ""

    function refresh() {
        entries = session.quotes();
        sources = session.sources();
    }

    function start() {
        refresh();
        list.forceActiveFocus();
    }

    // The text that a quote puts on the page.
    function citation(quote: var): string {
        const words = "“" + quote.text + "”";
        if (!quote.source)
            return words;
        return words + " [@" + quote.source + (quote.page ? ", p. " + quote.page : "") + "]";
    }

    function add() {
        if (session.saveQuote({ text: words.text, source: source, page: page.text }) === "") {
            error = session.lastError;
            return;
        }
        error = "";
        words.text = "";
        page.text = "";
        refresh();
    }

    Keys.onEscapePressed: closeRequested()

    Connections {
        target: panel.session

        function onEdited() {
            if (panel.visible)
                panel.refresh();
        }
    }

    Text {
        id: heading

        width: parent.width
        leftPadding: Theme.space.s
        text: "QUOTES"
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.small
        font.weight: Font.DemiBold
        font.letterSpacing: 1.2
        color: Theme.color.inkMuted
    }

    Text {
        visible: panel.entries.length === 0
        anchors.top: heading.bottom
        anchors.topMargin: Theme.space.l
        width: parent.width
        padding: Theme.space.s
        wrapMode: Text.Wrap
        text: "No quotes yet. Keep the words you want to cite here, with their source and page."
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    ListView {
        id: list

        objectName: "quoteList"
        anchors.top: heading.bottom
        anchors.topMargin: Theme.space.s
        anchors.bottom: form.top
        anchors.bottomMargin: Theme.space.s
        width: parent.width
        clip: true
        spacing: Theme.space.s
        boundsBehavior: Flickable.StopAtBounds
        // An empty list is no Tab stop. The ring marks the current quote.
        activeFocusOnTab: count > 0
        model: panel.entries
        highlight: Item {
            z: 2

            FocusRing {
                visible: list.activeFocus
            }
        }
        Keys.onReturnPressed: if (currentIndex >= 0) panel.inserted(panel.citation(panel.entries[currentIndex]))
        Keys.onEnterPressed: if (currentIndex >= 0) panel.inserted(panel.citation(panel.entries[currentIndex]))

        delegate: T.ItemDelegate {
            id: row

            required property var modelData
            required property int index

            objectName: "quote"
            width: ListView.view.width
            implicitHeight: implicitContentHeight + topPadding + bottomPadding
            padding: Theme.space.s
            Accessible.name: "Quote: " + modelData.text
            onClicked: list.currentIndex = index
            onDoubleClicked: panel.inserted(panel.citation(modelData))
            // A drag carries the quote and its citation as plain text.
            Drag.active: drag.active
            Drag.dragType: Drag.Automatic
            Drag.supportedActions: Qt.CopyAction
            Drag.mimeData: ({ "text/plain": panel.citation(modelData) })

            DragHandler {
                id: drag

                target: null
            }

            contentItem: Column {
                spacing: 2

                Text {
                    width: parent.width
                    text: "“" + row.modelData.text + "”"
                    wrapMode: Text.Wrap
                    maximumLineCount: 6
                    elide: Text.ElideRight
                    font.family: Theme.type.reading
                    font.italic: true
                    font.pixelSize: Theme.type.body + 1
                    color: Theme.color.ink
                }

                Text {
                    width: parent.width
                    text: (row.modelData.label || row.modelData.source || "No source")
                          + (row.modelData.page ? ", p. " + row.modelData.page : "")
                    elide: Text.ElideRight
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.small
                    color: Theme.color.inkMuted
                }

                Row {
                    spacing: Theme.space.xs

                    Button {
                        objectName: "insertQuote"
                        text: "Insert"
                        Accessible.name: "Insert the quote"
                        onClicked: panel.inserted(panel.citation(row.modelData))
                    }

                    Button {
                        text: "Remove"
                        Accessible.name: "Remove the quote"
                        onClicked: panel.session.removeQuote(row.modelData.id)
                    }
                }
            }

            background: Rectangle {
                radius: Theme.radius.small
                color: row.hovered ? Theme.color.surface : "transparent"
                border.width: 1
                border.color: Theme.color.rule
            }
        }
    }

    // A new quote: its words, its source, and its page.
    Column {
        id: form

        anchors.bottom: parent.bottom
        width: parent.width
        spacing: Theme.space.xs

        TextArea {
            id: words

            objectName: "quoteWords"
            width: parent.width
            placeholderText: "The words of a new quote"
            Accessible.name: "Words of the quote"
        }

        Button {
            objectName: "quoteSource"
            width: parent.width
            text: {
                const chosen = panel.sources.find(each => each.id === panel.source);
                return chosen ? "Source: " + (chosen.label || chosen.id) : "Choose the source";
            }
            onClicked: sourceMenu.popup()

            Menu {
                id: sourceMenu

                Instantiator {
                    model: panel.sources

                    delegate: T.Action {
                        required property var modelData

                        text: modelData.label || modelData.id
                        onTriggered: panel.source = modelData.id
                    }

                    onObjectAdded: (index, action) => sourceMenu.insertAction(index, action as T.Action)
                    onObjectRemoved: (index, action) => sourceMenu.removeAction(action as T.Action)
                }
            }
        }

        Row {
            width: parent.width
            spacing: Theme.space.xs

            TextField {
                id: page

                objectName: "quotePage"
                width: parent.width - addButton.width - parent.spacing
                placeholderText: "Page"
                Accessible.name: "Page of the quote"
                onAccepted: panel.add()
            }

            Button {
                id: addButton

                objectName: "addQuote"
                kind: "primary"
                text: "Add"
                Accessible.name: "Add the quote"
                enabled: words.text.trim() !== ""
                onClicked: panel.add()
            }
        }

        Text {
            visible: panel.error !== ""
            width: parent.width
            wrapMode: Text.Wrap
            text: panel.error
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            color: Theme.color.accent
        }
    }
}
