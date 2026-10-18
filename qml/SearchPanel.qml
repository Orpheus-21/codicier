pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Full text search over the project. The results follow the typing. Each
// result shows the title and a snippet with the match in bold. The .* toggle
// or Alt+R makes the query a regular expression.
Item {
    id: panel

    required property ProjectSession session

    // A click or Enter on a result.
    signal opened(id: string, query: string, regex: bool)
    // Escape in the panel.
    signal closeRequested

    property var results: []
    property bool regex: false
    readonly property string error: regex ? session.patternError(field.text) : ""

    onRegexChanged: searchTimer.restart()

    function start() {
        field.forceActiveFocus();
        field.selectAll();
    }

    function openRow(row: int) {
        if (row >= 0 && row < results.length)
            opened(results[row].id, field.text, regex);
    }

    Keys.onEscapePressed: closeRequested()

    // A short wait, so a fast typist does not search at each key.
    Timer {
        id: searchTimer

        interval: 120
        onTriggered: {
            panel.results = panel.session.search(field.text, panel.regex);
            list.currentIndex = panel.results.length > 0 ? 0 : -1;
        }
    }

    TextField {
        id: field

        objectName: "searchField"
        width: parent.width
        iconName: "search"
        placeholderText: panel.regex ? "Regular expression" : "Search the project"
        rightPadding: Theme.space.s + regexButton.width
        onTextChanged: searchTimer.restart()
        Keys.onPressed: event => {
            if (event.key === Qt.Key_R && event.modifiers === Qt.AltModifier) {
                panel.regex = !panel.regex;
                event.accepted = true;
            }
        }
        Keys.onReturnPressed: panel.openRow(list.currentIndex)
        Keys.onEnterPressed: panel.openRow(list.currentIndex)
        Keys.onDownPressed: {
            if (panel.results.length > 0) {
                list.currentIndex = Math.min(list.currentIndex + 1, panel.results.length - 1);
                list.positionViewAtIndex(list.currentIndex, ListView.Contain);
            }
        }
        Keys.onUpPressed: {
            list.currentIndex = Math.max(list.currentIndex - 1, 0);
            list.positionViewAtIndex(list.currentIndex, ListView.Contain);
        }
    }

    T.Button {
        id: regexButton

        anchors.right: field.right
        anchors.rightMargin: 4
        anchors.verticalCenter: field.verticalCenter
        implicitWidth: 28
        implicitHeight: 26
        checkable: true
        checked: panel.regex
        focusPolicy: Qt.NoFocus
        Accessible.name: "Regular expression"
        onToggled: panel.regex = checked

        contentItem: Text {
            text: ".*"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            font.weight: Font.Medium
            color: regexButton.checked ? Theme.color.ink
                   : regexButton.hovered ? Theme.color.ink : Theme.color.inkMuted
        }

        background: Rectangle {
            radius: Theme.radius.small
            color: regexButton.checked || regexButton.hovered ? Theme.color.selection : "transparent"
        }
    }

    // A pattern that is not valid: the reason, in place of the results.
    Text {
        objectName: "searchError"
        visible: panel.error !== ""
        anchors.top: field.bottom
        anchors.topMargin: Theme.space.s
        width: parent.width
        wrapMode: Text.Wrap
        text: panel.error
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.small
        color: Theme.color.accent
    }

    Text {
        visible: field.text.trim() !== "" && panel.results.length === 0 && !searchTimer.running
                 && panel.error === ""
        anchors.top: field.bottom
        anchors.topMargin: Theme.space.l
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        text: "No match"
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    ListView {
        id: list

        objectName: "searchResults"
        anchors.top: field.bottom
        anchors.topMargin: Theme.space.s
        anchors.bottom: parent.bottom
        width: parent.width
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: panel.results
        spacing: 2

        WheelHandler {
            onWheel: event => {
                const end = Math.max(0, list.contentHeight - list.height);
                list.contentY = Math.min(end, Math.max(0, list.contentY - event.angleDelta.y / 2));
            }
        }

        delegate: T.ItemDelegate {
            id: row

            required property var modelData
            required property int index

            width: ListView.view.width
            // The template gives no implicit height.
            implicitHeight: implicitContentHeight + topPadding + bottomPadding
            highlighted: ListView.isCurrentItem
            padding: Theme.space.s
            onClicked: panel.openRow(index)

            contentItem: Column {
                spacing: 2

                Text {
                    width: parent.width
                    text: row.modelData.title
                    elide: Text.ElideRight
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.body
                    font.weight: Font.Medium
                    color: Theme.color.ink
                }

                Text {
                    visible: text !== ""
                    width: parent.width
                    text: row.modelData.snippet
                    textFormat: Text.StyledText
                    wrapMode: Text.Wrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.small
                    color: Theme.color.inkMuted
                }
            }

            background: Rectangle {
                radius: Theme.radius.small
                color: row.highlighted || row.down ? Theme.color.selection : "transparent"

                Rectangle {
                    anchors.fill: parent
                    visible: row.hovered && !row.highlighted && !row.down
                    radius: parent.radius
                    color: Theme.color.selection
                    opacity: 0.6
                }
            }
        }
    }
}
