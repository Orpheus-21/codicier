pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Full text search over the project. The results follow the typing. Each
// result shows the title and a snippet with the match in bold. The .* toggle
// or Alt+R makes the query a regular expression. In replace mode, the panel
// shows every match in the manuscript with the change, and a button makes
// the change after a snapshot of each scene.
Item {
    id: panel

    required property ProjectSession session

    // A click or Enter on a result.
    signal opened(id: string, query: string, regex: bool)
    // Escape in the panel.
    signal closeRequested

    property var results: []
    property bool regex: false
    property bool replacing: false
    // The replace preview: one entry for each scene with matches.
    property var preview: []
    readonly property int matchCount: preview.reduce((sum, scene) => sum + scene.count, 0)
    // The result of the last replace, or "".
    property string doneText: ""
    readonly property string error: regex ? session.patternError(field.text) : ""

    onRegexChanged: searchTimer.restart()
    onReplacingChanged: searchTimer.restart()

    function start() {
        field.forceActiveFocus();
        field.selectAll();
    }

    function replaceAll() {
        const scenes = preview.length;
        const count = session.replaceAll(field.text, replaceField.text, regex);
        doneText = count < 0 ? "The replace stopped with an error. Each changed scene has a snapshot."
                             : "Replaced " + count + " in " + scenes + (scenes === 1 ? " scene" : " scenes")
                               + ". Each scene has a snapshot named Before replace.";
        searchTimer.restart();
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
            if (panel.replacing) {
                panel.preview = panel.session.replacePreview(field.text, replaceField.text, panel.regex);
                return;
            }
            panel.results = panel.session.search(field.text, panel.regex);
            list.currentIndex = panel.results.length > 0 ? 0 : -1;
        }
    }

    Column {
        id: fields

        width: parent.width
        spacing: Theme.space.s

        TextField {
            id: field

            objectName: "searchField"
            width: parent.width
            iconName: "search"
            placeholderText: panel.regex ? "Regular expression" : "Search the project"
            rightPadding: Theme.space.s + regexButton.width
            onTextChanged: {
                panel.doneText = "";
                searchTimer.restart();
            }
            Keys.onPressed: event => {
                if (event.key === Qt.Key_R && event.modifiers === Qt.AltModifier) {
                    panel.regex = !panel.regex;
                    event.accepted = true;
                }
            }
            Keys.onReturnPressed: panel.openRow(list.currentIndex)
            Keys.onEnterPressed: panel.openRow(list.currentIndex)
            Keys.onTabPressed: (panel.replacing ? replaceField : field).forceActiveFocus()
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

        TextField {
            id: replaceField

            objectName: "replaceField"
            visible: panel.replacing
            width: parent.width
            iconName: "pencil"
            placeholderText: panel.regex ? "Replace with, \\1 for a group" : "Replace with"
            onTextChanged: {
                panel.doneText = "";
                searchTimer.restart();
            }
            Keys.onTabPressed: field.forceActiveFocus()
        }

        // A pattern that is not valid: the reason, in place of the results.
        Text {
            objectName: "searchError"
            visible: panel.error !== ""
            width: parent.width
            wrapMode: Text.Wrap
            text: panel.error
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            color: Theme.color.accent
        }

        Text {
            objectName: "replaceDone"
            visible: panel.replacing && panel.doneText !== ""
            width: parent.width
            wrapMode: Text.Wrap
            text: panel.doneText
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            color: Theme.color.inkMuted
        }
    }

    T.Button {
        id: regexButton

        x: field.width - width - 4
        y: (field.height - height) / 2
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

    Text {
        visible: field.text.trim() !== "" && !searchTimer.running && panel.error === ""
                 && (panel.replacing ? panel.preview.length === 0 && panel.doneText === ""
                                     : panel.results.length === 0)
        anchors.top: fields.bottom
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
        visible: !panel.replacing
        anchors.top: fields.bottom
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

    // Every match with its change, scene by scene.
    ListView {
        id: previewList

        objectName: "replacePreview"
        visible: panel.replacing
        anchors.top: fields.bottom
        anchors.topMargin: Theme.space.s
        anchors.bottom: replaceButton.top
        anchors.bottomMargin: Theme.space.s
        width: parent.width
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: panel.preview
        spacing: Theme.space.m

        WheelHandler {
            onWheel: event => {
                const end = Math.max(0, previewList.contentHeight - previewList.height);
                previewList.contentY = Math.min(end, Math.max(0, previewList.contentY
                                                                    - event.angleDelta.y / 2));
            }
        }

        delegate: Column {
            id: scene

            required property var modelData

            width: ListView.view.width
            leftPadding: Theme.space.s
            rightPadding: Theme.space.s
            spacing: Theme.space.xs

            Item {
                width: scene.width - scene.leftPadding - scene.rightPadding
                height: sceneTitle.height

                Text {
                    id: sceneTitle

                    width: parent.width - sceneCount.width - Theme.space.s
                    text: scene.modelData.title
                    elide: Text.ElideRight
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.body
                    font.weight: Font.Medium
                    color: Theme.color.ink
                }

                Text {
                    id: sceneCount

                    anchors.right: parent.right
                    anchors.baseline: sceneTitle.baseline
                    text: scene.modelData.count
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.small
                    color: Theme.color.inkMuted
                }
            }

            Repeater {
                model: scene.modelData.lines

                Text {
                    required property string modelData

                    width: scene.width - scene.leftPadding - scene.rightPadding
                    leftPadding: Theme.space.s
                    text: modelData
                    textFormat: Text.StyledText
                    wrapMode: Text.Wrap
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.small
                    color: Theme.color.inkMuted
                }
            }
        }
    }

    Button {
        id: replaceButton

        objectName: "replaceButton"
        visible: panel.replacing
        anchors.bottom: parent.bottom
        width: parent.width
        height: visible ? implicitHeight : 0
        kind: "primary"
        enabled: panel.matchCount > 0 && !searchTimer.running
        text: panel.matchCount === 0 ? "Replace"
              : "Replace " + panel.matchCount + " in " + panel.preview.length
                + (panel.preview.length === 1 ? " scene" : " scenes")
        onClicked: panel.replaceAll()
    }
}
