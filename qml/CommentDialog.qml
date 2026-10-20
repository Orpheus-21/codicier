pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Adds a comment on the selected words of a text.
T.Popup {
    id: dialog

    required property ProjectSession session
    property string itemId: ""
    property int start: 0
    property string quote: ""

    function ask(id: string, at: int, words: string) {
        itemId = id;
        start = at;
        quote = words;
        open();
    }

    function save() {
        if (session.addComment(itemId, start, quote, field.text))
            close();
    }

    implicitWidth: 480
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    padding: Theme.space.xl
    modal: true
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    onAboutToShow: {
        field.text = "";
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

    contentItem: Column {
        spacing: Theme.space.l

        Text {
            text: "Comment"
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.title
            color: Theme.color.ink
        }

        Text {
            width: parent.width
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
            text: "On: " + dialog.quote
            font.family: Theme.type.reading
            font.italic: true
            font.pixelSize: Theme.type.body + 1
            color: Theme.color.inkMuted
        }

        TextArea {
            id: field

            objectName: "commentText"
            width: parent.width
            Accessible.name: "Comment"
            placeholderText: "What to change, or a question for later"
        }

        Row {
            anchors.right: parent.right
            spacing: Theme.space.s

            Button {
                text: "Cancel"
                onClicked: dialog.close()
            }

            Button {
                objectName: "saveComment"
                kind: "primary"
                text: "Add the comment"
                enabled: field.text.trim() !== ""
                onClicked: dialog.save()
            }
        }
    }
}
