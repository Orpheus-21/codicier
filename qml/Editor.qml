pragma ComponentBehavior: Bound

import QtQml
import QtQuick
import QtQuick.Templates as T

// The page: the text of the open document in the reading font, in a column
// of the measure width. The document saves itself one second after an edit.
Item {
    id: editorArea

    required property Document document
    // The session gives the entities for links. Without it, links are text.
    property ProjectSession session: null

    // Ctrl+click on a [[link]] asks to open its entity.
    signal linkOpened(id: string)

    // The open [[ before the cursor: its start, the typed name, the matches,
    // and the chosen match. linkStart is -1 when no list shows.
    property int linkStart: -1
    property var linkMatches: []
    property int linkIndex: 0

    function updateLink() {
        const at = textEdit.cursorPosition;
        const before = textEdit.getText(Math.max(0, at - 80), at);
        const open = before.match(/\[\[([^\[\]\n]*)$/);
        if (!open || !session) {
            linkStart = -1;
            return;
        }
        const query = open[1].toLowerCase();
        const kinds = session.entityTypes().map(type => type.kind);
        linkMatches = session.items()
            .filter(item => kinds.includes(item.kind) && item.title.toLowerCase().includes(query))
            .slice(0, 8);
        linkIndex = 0;
        linkStart = linkMatches.length > 0 ? at - open[1].length : -1;
    }

    // The word under a right click: its start, its end, and the suggestions.
    property int wordStart: -1
    property int wordEnd: -1
    property var wordSuggestions: []

    // Opens the menu of spelling suggestions for the word at position.
    function suggestAt(position: int, x: real, y: real) {
        if (!session || !session.spellCheck)
            return;
        const text = textEdit.text;
        // The QML engine has no \p{L}, so a letter is a character with two
        // cases. An apostrophe inside a word counts too.
        const isLetter = c => c !== undefined && (c.toLowerCase() !== c.toUpperCase() || c === "'" || c === "’");
        let start = position;
        let end = position;
        while (start > 0 && isLetter(text[start - 1]))
            --start;
        while (end < text.length && isLetter(text[end]))
            ++end;
        if (start === end)
            return;
        wordStart = start;
        wordEnd = end;
        const word = text.slice(start, end);
        if (session.isWordCorrect(word))
            return;
        wordSuggestions = session.suggestions(word).slice(0, 7);
        spellMenu.popup(textEdit, x, y);
    }

    function replaceWord(word: string) {
        textEdit.remove(wordStart, wordEnd);
        textEdit.insert(wordStart, word);
        textEdit.cursorPosition = wordStart + word.length;
    }

    Menu {
        id: spellMenu

        objectName: "spellMenu"

        Instantiator {
            model: editorArea.wordSuggestions

            delegate: T.Action {
                required property string modelData

                text: modelData
                onTriggered: editorArea.replaceWord(modelData)
            }

            onObjectAdded: (index, action) => spellMenu.insertAction(index, action as T.Action)
            onObjectRemoved: (index, action) => spellMenu.removeAction(action as T.Action)
        }

        T.Action {
            text: "Add to the project dictionary"
            onTriggered: editorArea.session.addToDictionary(textEdit.text.slice(editorArea.wordStart, editorArea.wordEnd))
        }
    }

    // Puts the chosen title in place of the typed name and closes the link.
    function completeLink() {
        // The edits below call updateLink, so keep the values first.
        const title = linkMatches[linkIndex].title;
        const start = linkStart;
        const at = textEdit.cursorPosition;
        const closed = textEdit.getText(at, at + 2) === "]]";
        textEdit.remove(start, at);
        textEdit.insert(start, title + (closed ? "" : "]]"));
        textEdit.cursorPosition = start + title.length + 2;
        linkStart = -1;
    }
    // The place of the cursor in the text, for a split.
    readonly property int cursorPosition: textEdit.cursorPosition
    // Typewriter scroll: the line with the cursor stays at one height.
    property bool typewriter: false
    // That height, as a part of the page height.
    readonly property real typewriterLine: 0.45

    // A conflict: another program changed the file while the text was not
    // saved. The author chooses, and autosave waits.
    Rectangle {
        id: conflictBar

        visible: editorArea.document.conflict
        width: parent.width
        height: visible ? 52 : 0
        color: Theme.color.surface

        Row {
            anchors.centerIn: parent
            spacing: Theme.space.l

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "This text changed in another program."
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.body
                color: Theme.color.ink
            }

            Button {
                text: "Keep mine"
                kind: "primary"
                onClicked: editorArea.document.keepMine()
            }

            Button {
                text: "Use the file"
                onClicked: editorArea.document.useFile()
            }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.color.rule
        }
    }

    Flickable {
        id: page

        objectName: "page"

        anchors.top: conflictBar.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        contentWidth: width
        contentHeight: textEdit.implicitHeight + 2 * Theme.space.xxxl
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        // Keeps the cursor line in view while the author types.
        // In typewriter mode, space above and below the text lets the first and
        // the last lines reach the typewriter line.
        topMargin: editorArea.typewriter ? height * editorArea.typewriterLine : 0
        bottomMargin: editorArea.typewriter ? height * (1 - editorArea.typewriterLine) : 0

        Behavior on contentY {
            enabled: editorArea.typewriter
            NumberAnimation {
                duration: Theme.motion.fast
                easing.type: Theme.motion.easing
            }
        }

        function follow(rect: rect) {
            const top = rect.y + Theme.space.xxxl;
            if (editorArea.typewriter)
                contentY = top - height * editorArea.typewriterLine;
            else if (top < contentY)
                contentY = top;
            else if (top + rect.height > contentY + height)
                contentY = top + rect.height - height;
        }

        PageText {
            id: textEdit

            objectName: "editor"
            Accessible.name: "Text of the page"
            Accessible.multiLine: true
            document: editorArea.document
            availableWidth: page.width - 2 * Theme.space.xl
            y: Theme.space.xxxl
            x: Math.max(Theme.space.xl, (page.width - width) / 2)
            visible: editorArea.document.file !== ""
            onCursorRectangleChanged: page.follow(cursorRectangle)
            onTextChanged: editorArea.updateLink()
            onCursorPositionChanged: editorArea.updateLink()
            Keys.onPressed: event => {
                if (editorArea.linkStart < 0)
                    return;
                const count = editorArea.linkMatches.length;
                if (event.key === Qt.Key_Down) {
                    editorArea.linkIndex = (editorArea.linkIndex + 1) % count;
                } else if (event.key === Qt.Key_Up) {
                    editorArea.linkIndex = (editorArea.linkIndex + count - 1) % count;
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                           || event.key === Qt.Key_Tab) {
                    editorArea.completeLink();
                } else if (event.key === Qt.Key_Escape) {
                    editorArea.linkStart = -1;
                } else {
                    return;
                }
                event.accepted = true;
            }

            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: eventPoint => editorArea.suggestAt(
                    textEdit.positionAt(eventPoint.position.x, eventPoint.position.y),
                    eventPoint.position.x, eventPoint.position.y)
            }

            TapHandler {
                acceptedModifiers: Qt.ControlModifier
                onTapped: eventPoint => {
                    if (!editorArea.session)
                        return;
                    const at = textEdit.positionAt(eventPoint.position.x, eventPoint.position.y);
                    const id = editorArea.session.entityAt(textEdit.text, at);
                    if (id !== "")
                        editorArea.linkOpened(id);
                }
            }
        }
    }

    Text {
        visible: editorArea.document.file === ""
        anchors.centerIn: parent
        text: "Choose a scene in the binder."
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    Text {
        visible: editorArea.document.error !== ""
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.space.l
        anchors.horizontalCenter: parent.horizontalCenter
        text: editorArea.document.error
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.accent
    }

    // Puts the keyboard focus on the page.
    function focusText() {
        textEdit.forceActiveFocus();
    }

    // Selects length characters from start and puts the focus on the page.
    // A start of -1 only puts the focus on the page.
    function selectRange(start: int, length: int) {
        textEdit.forceActiveFocus();
        if (start < 0)
            return;
        textEdit.select(start, start + length);
        placeCursor();
    }

    // Brings the cursor line to its place, as after a change of mode.
    function placeCursor() {
        page.follow(textEdit.cursorRectangle);
    }

    onTypewriterChanged: Qt.callLater(placeCursor)

    // The list of entities for the open [[, under the cursor.
    Rectangle {
        id: linkList

        readonly property point place: textEdit.mapToItem(editorArea, textEdit.cursorRectangle.x,
                                                          textEdit.cursorRectangle.y + textEdit.cursorRectangle.height)

        objectName: "linkList"
        visible: editorArea.linkStart >= 0
        x: Math.min(place.x, editorArea.width - width - Theme.space.s)
        y: place.y + Theme.space.xs
        z: 3
        width: 260
        height: links.height + 2 * Theme.space.xs
        radius: Theme.radius.small
        color: Theme.color.paper
        border.width: 1
        border.color: Theme.color.rule

        Column {
            id: links

            x: Theme.space.xs
            y: Theme.space.xs
            width: parent.width - 2 * Theme.space.xs

            Repeater {
                model: editorArea.linkMatches

                Rectangle {
                    required property var modelData
                    required property int index

                    width: links.width
                    height: 28
                    radius: Theme.radius.small
                    color: index === editorArea.linkIndex ? Theme.color.selection : "transparent"

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        x: Theme.space.s
                        width: parent.width - 2 * Theme.space.s
                        text: parent.modelData.title
                        elide: Text.ElideRight
                        font.family: Theme.type.ui
                        font.pixelSize: Theme.type.body
                        color: Theme.color.ink
                    }
                }
            }
        }
    }
}
