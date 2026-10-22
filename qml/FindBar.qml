pragma ComponentBehavior: Bound

import QtQuick

// Finds text in the open page, without regard to case. Enter goes to the
// next match, Shift+Enter to the one before, and Escape closes the bar and
// puts the cursor back on the page. The replace row replaces the current
// match or all matches.
Rectangle {
    id: bar

    // The text area of the page.
    required property TextEdit target
    property bool replacing: false
    // The start of each match, and the current one.
    property var hits: []
    property int current: -1
    property string message: ""

    signal closed

    function open(withReplace: bool) {
        replacing = withReplace;
        visible = true;
        const chosen = target.selectedText;
        if (chosen !== "" && !chosen.includes("\u2029"))
            findField.text = chosen;
        findField.forceActiveFocus();
        findField.selectAll();
        search(target.selectionStart);
    }

    function close() {
        visible = false;
        message = "";
        closed();
    }

    // Finds all matches. The current match is the first one at or after from.
    function search(from: int) {
        const query = findField.text.toLowerCase();
        let found = [];
        if (query !== "") {
            const text = target.getText(0, target.length).toLowerCase();
            for (let at = text.indexOf(query); at >= 0; at = text.indexOf(query, at + query.length))
                found.push(at);
        }
        hits = found;
        const next = found.findIndex(at => at >= from);
        show(found.length === 0 ? -1 : next >= 0 ? next : 0);
    }

    function show(index: int) {
        current = index;
        if (index >= 0)
            target.select(hits[index], hits[index] + findField.text.length);
    }

    function step(delta: int) {
        if (hits.length > 0)
            show((current + delta + hits.length) % hits.length);
    }

    function replaceCurrent() {
        if (current < 0)
            return;
        const at = hits[current];
        target.remove(at, at + findField.text.length);
        target.insert(at, replaceField.text);
        search(at + replaceField.text.length);
    }

    function replaceAll() {
        const count = hits.length;
        // From the end, so the earlier starts stay right.
        for (let i = count - 1; i >= 0; --i) {
            target.remove(hits[i], hits[i] + findField.text.length);
            target.insert(hits[i], replaceField.text);
        }
        search(0);
        message = count === 1 ? "1 replaced" : count + " replaced";
    }

    visible: false
    width: 420
    height: column.implicitHeight + 2 * Theme.space.s
    radius: Theme.radius.medium
    color: Theme.color.surface
    border.width: 1
    border.color: Theme.color.rule

    Column {
        id: column

        x: Theme.space.s
        y: Theme.space.s
        width: parent.width - 2 * Theme.space.s
        spacing: Theme.space.xs

        Row {
            spacing: Theme.space.s

            TextField {
                id: findField

                objectName: "findField"
                width: column.width - countText.width - Theme.space.s
                iconName: "search"
                placeholderText: "Find in this scene"
                onTextChanged: bar.search(bar.target.selectionStart)
                Keys.onReturnPressed: event => bar.step(event.modifiers & Qt.ShiftModifier ? -1 : 1)
                Keys.onEnterPressed: event => bar.step(event.modifiers & Qt.ShiftModifier ? -1 : 1)
                Keys.onEscapePressed: bar.close()
            }

            Text {
                id: countText

                objectName: "findCount"
                width: 80
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignRight
                text: bar.message !== "" ? bar.message
                    : findField.text === "" ? ""
                    : bar.hits.length === 0 ? "No match"
                    : (bar.current + 1) + " of " + bar.hits.length
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.small
                color: Theme.color.inkMuted
            }
        }

        Row {
            visible: bar.replacing
            spacing: Theme.space.s

            TextField {
                id: replaceField

                objectName: "replaceField"
                width: column.width - replaceButton.width - allButton.width - 2 * Theme.space.s
                placeholderText: "Replace with"
                Keys.onReturnPressed: bar.replaceCurrent()
                Keys.onEnterPressed: bar.replaceCurrent()
                Keys.onEscapePressed: bar.close()
            }

            Button {
                id: replaceButton

                text: "Replace"
                enabled: bar.current >= 0
                onClicked: bar.replaceCurrent()
            }

            Button {
                id: allButton

                objectName: "replaceAll"
                text: "All"
                enabled: bar.hits.length > 0
                onClicked: bar.replaceAll()
            }
        }
    }
}
