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

    // The open [[ or [@ before the cursor: its start, the typed name, the
    // matches, and the chosen match. linkStart is -1 when no list shows. A
    // citation lists the sources by their key, as pandoc writes them.
    property int linkStart: -1
    property var linkMatches: []
    property int linkIndex: 0
    property bool citing: false
    // The characters of a citation key.
    readonly property string keyChars: "A-Za-z0-9_:.#$%&+?<>~/-"

    function updateLink() {
        const at = textEdit.cursorPosition;
        const before = textEdit.getText(Math.max(0, at - 80), at);
        const cite = before.match(new RegExp("(?:\\[|;\\s?)@([" + keyChars + "]*)$"));
        if (cite && session) {
            const query = cite[1].toLowerCase();
            linkMatches = session.sources()
                .filter(source => source.id.toLowerCase().includes(query) || source.label.toLowerCase().includes(query))
                .slice(0, 8)
                .map(source => ({ title: source.label || source.id, key: source.id }));
            citing = true;
            linkIndex = 0;
            linkStart = linkMatches.length > 0 ? at - cite[1].length : -1;
            return;
        }
        citing = false;
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
    // The entity of the codex under the pointer, and the place of the pointer
    // on the page.
    property string hoverId: ""
    // The source of the citation key under the pointer, or null.
    property var hoverSource: null

    // The citation key at position, as in [@key] or [see @key, p. 3], or "".
    function citationAt(position: int): string {
        const text = textEdit.text;
        const isKey = c => c !== undefined && new RegExp("[" + keyChars + "]").test(c);
        let start = text[position] === "@" ? position + 1 : position;
        let end = start;
        while (start > 0 && isKey(text[start - 1]))
            --start;
        while (end < text.length && isKey(text[end]))
            ++end;
        // The "@" starts the key, after a bracket, a space, or a semicolon,
        // so an e-mail address is no citation.
        if (start < 2 || start === end || text[start - 1] !== "@" || !"[ ;-".includes(text[start - 2]))
            return "";
        return text.slice(start, end);
    }
    property point hoverPoint: Qt.point(0, 0)

    Timer {
        id: hoverTimer

        interval: 400
        onTriggered: {
            if (!editorArea.session)
                return;
            const at = textEdit.positionAt(editorArea.hoverPoint.x, editorArea.hoverPoint.y);
            editorArea.hoverId = editorArea.session.entityAt(textEdit.text, at);
            const key = editorArea.hoverId === "" ? editorArea.citationAt(at) : "";
            editorArea.hoverSource = key === "" ? null
                : editorArea.session.sources().find(source => source.id === key) ?? null;
        }
    }
    property var wordSynonyms: []

    // Opens the menu of spelling suggestions for a misspelled word at
    // position, or the menu of synonyms for a correct word.
    function suggestAt(position: int, x: real, y: real) {
        if (!session)
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
        if (session.spellCheck && !session.isWordCorrect(word)) {
            wordSuggestions = session.suggestions(word).slice(0, 7);
            spellMenu.popup(textEdit, x, y);
            return;
        }
        wordSynonyms = session.synonyms(word);
        if (wordSynonyms.length > 0)
            synonymMenu.popup(textEdit, x, y);
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

    Menu {
        id: synonymMenu

        objectName: "synonymMenu"

        Instantiator {
            model: editorArea.wordSynonyms

            delegate: T.Action {
                required property string modelData

                text: modelData
                onTriggered: editorArea.replaceWord(modelData)
            }

            onObjectAdded: (index, action) => synonymMenu.insertAction(index, action as T.Action)
            onObjectRemoved: (index, action) => synonymMenu.removeAction(action as T.Action)
        }
    }

    // Puts the chosen title in place of the typed name and closes the link.
    function completeLink() {
        // The edits below call updateLink, so keep the values first.
        const title = linkMatches[linkIndex].title;
        const start = linkStart;
        const at = textEdit.cursorPosition;
        if (citing) {
            // A citation ends at "]", or goes on with "; @" or a page.
            const key = linkMatches[linkIndex].key;
            const after = textEdit.getText(at, at + 1);
            const goesOn = after === ";" || after === ",";
            textEdit.remove(start, at);
            textEdit.insert(start, key + (after === "]" || goesOn ? "" : "]"));
            // The cursor goes past the "]", or stays before the "; @".
            textEdit.cursorPosition = start + key.length + (goesOn ? 0 : 1);
            linkStart = -1;
            return;
        }
        const closed = textEdit.getText(at, at + 2) === "]]";
        textEdit.remove(start, at);
        textEdit.insert(start, title + (closed ? "" : "]]"));
        textEdit.cursorPosition = start + title.length + 2;
        linkStart = -1;
    }
    // The place of the cursor in the text, for a split.
    readonly property int cursorPosition: textEdit.cursorPosition
    // The scroll place of the page, kept across a restart.
    readonly property real scroll: page.contentY
    // The selection on the page, for a comment.
    readonly property int selectionStart: textEdit.selectionStart
    readonly property string selectedText: textEdit.selectedText
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
                text: "Another program changed this file."
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
                text: "Load theirs"
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
        // The find bar sits above the text, not on it.
        anchors.topMargin: findBar.visible ? findBar.height + Theme.space.s : 0
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

        // A quiet hint on an empty page.
        Text {
            objectName: "emptyHint"
            visible: textEdit.visible && textEdit.length === 0 && textEdit.preeditText === ""
            x: textEdit.x
            y: textEdit.y
            text: "Start writing"
            font: textEdit.font
            color: Theme.color.inkMuted
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
                // The key types its character first. The change comes after,
                // as its own step, so one undo gives back the typed text.
                if (Settings.typography !== 0 && event.text.length === 1 && "\"'-.".includes(event.text)
                        && !(event.modifiers & Qt.ControlModifier)) {
                    const at = textEdit.selectionStart;
                    const typed = event.text;
                    Qt.callLater(() => editorArea.smarten(at, typed));
                }
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

            // A quote dragged from the quote bank lands where it drops.
            DropArea {
                anchors.fill: parent
                enabled: !textEdit.readOnly
                onDropped: drop => {
                    if (!drop.hasText)
                        return;
                    textEdit.insert(textEdit.positionAt(drop.x, drop.y), drop.text);
                    drop.acceptProposedAction();
                }
            }

            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: eventPoint => editorArea.suggestAt(
                    textEdit.positionAt(eventPoint.position.x, eventPoint.position.y),
                    eventPoint.position.x, eventPoint.position.y)
            }

            // A pause over a name of the codex shows its card.
            HoverHandler {
                onPointChanged: {
                    editorArea.hoverPoint = point.position;
                    hoverTimer.restart();
                }
                onHoveredChanged: {
                    if (!hovered) {
                        hoverTimer.stop();
                        editorArea.hoverId = "";
                        editorArea.hoverSource = null;
                    }
                }
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

    // Changes the character typed at "at" to its typographic form: curly
    // quotes, an em dash for "--", and an ellipsis for "...".
    function smarten(at: int, typed: string) {
        if (textEdit.getText(at, at + 1) !== typed)
            return;
        const before = textEdit.getText(Math.max(0, at - 2), at);
        // A quote opens at the start, after a space, or after an opening mark.
        const opens = before === "" || /[\s\u2029(\[{\u2014\u2013\u201C\u2018]/.test(before.slice(-1));
        const smart = Settings.typography;
        if (typed === "\"" && (smart & 1))
            editorArea.document.replace(at, at + 1, opens ? "\u201C" : "\u201D");
        else if (typed === "'" && (smart & 1))
            editorArea.document.replace(at, at + 1, opens ? "\u2018" : "\u2019");
        else if (typed === "-" && (smart & 2) && before.endsWith("-"))
            editorArea.document.replace(at - 1, at + 1, "\u2014");
        else if (typed === "." && (smart & 4) && before === "..")
            editorArea.document.replace(at - 2, at + 1, "\u2026");
    }

    // The number of mark characters right before at (back) or right from
    // at, up to 3.
    function runAt(at: int, back: bool, mark: string): int {
        const text = back ? textEdit.getText(Math.max(0, at - 3), at) : textEdit.getText(at, at + 3);
        let count = 0;
        while (count < text.length && text[back ? text.length - 1 - count : count] === mark)
            ++count;
        return count;
    }

    // Puts mark ("**" for bold, "*" for italic, "==" for a highlight) around
    // the selection, or
    // takes it away when the selection already has it. With no selection,
    // the word at the cursor gets the mark. Outside a word, the mark goes in
    // as a pair with the cursor between.
    function toggleMark(mark: string) {
        if (!textEdit.activeFocus || editorArea.document.file === "")
            return;
        const isWordChar = c => c.toLowerCase() !== c.toUpperCase() || /[0-9'’]/.test(c);
        let start = textEdit.selectionStart;
        let end = textEdit.selectionEnd;
        if (start === end) {
            while (start > 0 && isWordChar(textEdit.getText(start - 1, start)))
                --start;
            while (end < textEdit.length && isWordChar(textEdit.getText(end, end + 1)))
                ++end;
            if (start === end) {
                textEdit.insert(start, mark + mark);
                textEdit.cursorPosition = start + mark.length;
                return;
            }
        }
        const n = mark.length;
        // Italic needs an odd run of stars on each side, bold or a highlight a
        // run of two or more.
        const has = count => n === 1 ? count % 2 === 1 : count >= 2;
        const runAround = (at, back) => editorArea.runAt(at, back, mark[0]);
        if (has(runAround(start, true)) && has(runAround(end, false))) {
            textEdit.remove(end, end + n);
            textEdit.remove(start - n, start);
            textEdit.select(start - n, end - n);
        } else if (end - start > 2 * n && has(runAround(start, false)) && has(runAround(end, true))) {
            textEdit.remove(end - n, end);
            textEdit.remove(start, start + n);
            textEdit.select(start, end - 2 * n);
        } else {
            textEdit.insert(end, mark);
            textEdit.insert(start, mark);
            textEdit.select(start + n, end + n);
        }
    }

    // Puts the keyboard focus on the page.
    function focusText() {
        textEdit.forceActiveFocus();
    }

    // Puts text at the cursor, in place of a selection, and puts the focus
    // on the page.
    function insertAtCursor(text: string) {
        if (textEdit.readOnly)
            return;
        textEdit.remove(textEdit.selectionStart, textEdit.selectionEnd);
        textEdit.insert(textEdit.cursorPosition, text);
        textEdit.forceActiveFocus();
    }

    // Puts the cursor and the scroll back where the author left them.
    function restorePlace(cursor: int, scroll: real) {
        textEdit.cursorPosition = Math.max(0, Math.min(cursor, textEdit.length));
        // The page follows the cursor first, so the scroll comes after.
        Qt.callLater(() => page.contentY = Math.max(0, scroll));
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
        // Below the line, or above it when the list does not fit below.
        y: place.y + Theme.space.xs + height <= editorArea.height - Theme.space.s
           ? place.y + Theme.space.xs
           : place.y - textEdit.cursorRectangle.height - Theme.space.xs - height
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

    FindBar {
        id: findBar

        objectName: "findBar"
        anchors.top: conflictBar.bottom
        anchors.right: parent.right
        anchors.topMargin: Theme.space.s
        anchors.rightMargin: Theme.space.l
        z: 5
        target: textEdit
        onClosed: textEdit.forceActiveFocus()
    }

    // Opens the find bar of the page, with the replace row when replace is true.
    function openFind(replace: bool) {
        if (editorArea.document.file !== "")
            findBar.open(replace);
    }

    // The card of a codex entry under the pointer.
    Rectangle {
        id: entityCard

        objectName: "entityCard"
        readonly property point place: textEdit.mapToItem(editorArea, editorArea.hoverPoint.x,
                                                          editorArea.hoverPoint.y)

        visible: editorArea.hoverId !== "" || editorArea.hoverSource !== null
        z: 6
        x: Math.min(place.x + Theme.space.m, editorArea.width - width - Theme.space.s)
        y: place.y + Theme.space.l
        width: 300
        height: cardColumn.implicitHeight + 2 * Theme.space.m
        radius: Theme.radius.medium
        color: Theme.color.surface
        border.width: 1
        border.color: Theme.color.rule

        Column {
            id: cardColumn

            x: Theme.space.m
            y: Theme.space.m
            width: parent.width - 2 * Theme.space.m
            spacing: Theme.space.xs

            Text {
                width: parent.width
                text: editorArea.hoverSource !== null ? editorArea.hoverSource.label
                    : entityCard.visible ? (editorArea.session.fields(editorArea.hoverId).title ?? "") : ""
                elide: Text.ElideRight
                font.family: Theme.type.reading
                font.pixelSize: Theme.type.large
                color: Theme.color.ink
            }

            Text {
                objectName: "entitySummary"
                visible: text !== ""
                width: parent.width
                wrapMode: Text.Wrap
                // A source shows where it was published and the place in it.
                text: editorArea.hoverSource !== null
                    ? [editorArea.hoverSource.authors, editorArea.hoverSource.container, editorArea.hoverSource.publisher,
                       editorArea.hoverSource.page ? "pp. " + editorArea.hoverSource.page : "", editorArea.hoverSource.url]
                        .filter(part => part).join(". ")
                    : entityCard.visible ? editorArea.session.summaryOf(editorArea.hoverId) : ""
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.body
                color: Theme.color.ink
            }

            Text {
                text: editorArea.hoverSource !== null ? "@" + editorArea.hoverSource.id : "Ctrl+click to open"
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.small
                color: Theme.color.inkMuted
            }
        }
    }
}
