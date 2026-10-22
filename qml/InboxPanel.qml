pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// The inbox: every open comment of the project, in binder order of the
// texts. A click opens the text at the quote. Resolve closes a comment.
Item {
    id: panel

    required property ProjectSession session

    // Escape in the panel.
    signal closeRequested
    // A click on a comment: the text and the place of its quote.
    signal opened(id: string, start: int, length: int)

    property var entries: []

    function refresh() {
        entries = session.comments("").filter(comment => !comment.resolved);
    }

    function start() {
        refresh();
        list.forceActiveFocus();
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
        text: "OPEN COMMENTS"
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
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: "No open comment. Select words on the page and press Ctrl+Alt+K to add one."
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    ListView {
        id: list

        objectName: "commentList"
        anchors.top: heading.bottom
        anchors.topMargin: Theme.space.s
        anchors.bottom: parent.bottom
        width: parent.width
        clip: true
        spacing: Theme.space.s
        boundsBehavior: Flickable.StopAtBounds
        // An empty list is no Tab stop. The ring marks the current entry.
        activeFocusOnTab: count > 0
        model: panel.entries
        highlight: Item {
            z: 2

            FocusRing {
                visible: list.activeFocus
            }
        }

        WheelHandler {
            onWheel: event => {
                const end = Math.max(0, list.contentHeight - list.height);
                list.contentY = Math.min(end, Math.max(0, list.contentY - event.angleDelta.y / 2));
            }
        }

        delegate: T.ItemDelegate {
            id: row

            required property var modelData

            objectName: "comment"
            width: ListView.view.width
            implicitHeight: implicitContentHeight + topPadding + bottomPadding
            padding: Theme.space.s
            Accessible.name: modelData.title + ": " + modelData.text
            onClicked: panel.opened(modelData.item, modelData.start, modelData.quote.length)

            contentItem: Column {
                spacing: 2

                Text {
                    width: parent.width
                    text: row.modelData.title
                    elide: Text.ElideRight
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.small
                    font.weight: Font.DemiBold
                    color: Theme.color.inkMuted
                }

                Text {
                    width: parent.width
                    text: row.modelData.text
                    wrapMode: Text.Wrap
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.body
                    color: Theme.color.ink
                }

                Text {
                    width: parent.width
                    text: row.modelData.start >= 0 ? "“" + row.modelData.quote + "”" : "The quote is gone from the text."
                    elide: Text.ElideRight
                    font.family: Theme.type.reading
                    font.italic: true
                    font.pixelSize: Theme.type.small + 1
                    color: Theme.color.inkMuted
                }

                Button {
                    objectName: "resolveComment"
                    text: "Resolve"
                    onClicked: panel.session.resolveComment(row.modelData.index, true)
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
}
