pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Makes a new project from a template: a title, a kind, and an author.
T.Popup {
    id: dialog

    required property ProjectList projects
    property string kind: "novel"
    property string error: ""

    readonly property var kinds: [
        { kind: "blank", label: "Blank", hint: "One scene and nothing else" },
        { kind: "short-story", label: "Short story", hint: "One scene to start" },
        { kind: "novella", label: "Novella", hint: "Chapters and scenes" },
        { kind: "novel", label: "Novel", hint: "Parts, chapters, scenes, front and back matter" },
        { kind: "series", label: "Series", hint: "A folder for each book" },
        { kind: "essay", label: "Essay", hint: "One draft" },
        { kind: "essay-collection", label: "Essay collection", hint: "An essay for each chapter" },
        { kind: "non-fiction", label: "Non-fiction book", hint: "Parts, chapters, sections, a bibliography" }
    ]

    function create() {
        dialog.error = dialog.projects.create(titleField.text, dialog.kind, authorField.text);
        if (dialog.error === "")
            dialog.close();
    }

    // The template has no size of its own: take the content plus padding.
    implicitWidth: 520
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    padding: Theme.space.xl
    modal: true
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    onAboutToShow: {
        titleField.text = "";
        dialog.error = "";
        dialog.kind = "novel";
        titleField.forceActiveFocus();
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
            text: "New project"
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.title
            color: Theme.color.ink
        }

        TextField {
            id: titleField

            width: parent.width
            placeholderText: "Title"
            onAccepted: dialog.create()
        }

        Column {
            width: parent.width
            spacing: 2

            Repeater {
                model: dialog.kinds

                ListItem {
                    required property var modelData

                    width: parent.width
                    height: 40
                    text: modelData.label
                    trailing: modelData.hint
                    highlighted: dialog.kind === modelData.kind
                    onClicked: dialog.kind = modelData.kind
                }
            }
        }

        TextField {
            id: authorField

            width: parent.width
            placeholderText: "Author (optional)"
            onAccepted: dialog.create()
        }

        Text {
            visible: dialog.error !== ""
            width: parent.width
            wrapMode: Text.Wrap
            text: dialog.error
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.accent
        }

        Row {
            anchors.right: parent.right
            spacing: Theme.space.s

            Button {
                text: "Cancel"
                onClicked: dialog.close()
            }

            Button {
                kind: "primary"
                text: "Create"
                onClicked: dialog.create()
            }
        }
    }
}
