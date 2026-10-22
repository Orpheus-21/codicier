pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Makes a new project from a template: a title, a kind, and an author.
T.Dialog {
    id: dialog

    title: "New project"

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
    implicitWidth: 720
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

        Row {
            width: parent.width
            spacing: Theme.space.l

            Column {
                id: kindList

                width: 240
                spacing: 2

                Repeater {
                    model: dialog.kinds

                    ListItem {
                        required property var modelData

                        width: parent.width
                        height: 40
                        text: modelData.label
                        highlighted: dialog.kind === modelData.kind
                        onClicked: dialog.kind = modelData.kind
                    }
                }
            }

            // The binder that the chosen template makes.
            Rectangle {
                width: parent.width - kindList.width - parent.spacing
                height: kindList.height
                radius: Theme.radius.small
                color: Theme.color.surface
                clip: true

                Column {
                    id: preview

                    objectName: "templatePreview"
                    readonly property var outline: dialog.projects.templateOutline(dialog.kind)
                    readonly property var sectionNames: ({ front: "Front matter", manuscript: "Manuscript",
                                                           back: "Back matter" })

                    x: Theme.space.m
                    y: Theme.space.m
                    width: parent.width - 2 * Theme.space.m
                    spacing: 2
                    Accessible.role: Accessible.List
                    Accessible.name: "Binder of the template"

                    Text {
                        width: parent.width
                        text: dialog.kinds.find(entry => entry.kind === dialog.kind)?.hint ?? ""
                        wrapMode: Text.Wrap
                        bottomPadding: Theme.space.s
                        font.family: Theme.type.ui
                        font.pixelSize: Theme.type.body
                        color: Theme.color.ink
                    }

                    Repeater {
                        model: preview.outline

                        Column {
                            id: entry

                            required property var modelData
                            required property int index

                            width: preview.width

                            Text {
                                visible: entry.index === 0
                                         || preview.outline[entry.index - 1].section !== entry.modelData.section
                                topPadding: entry.index === 0 ? 0 : Theme.space.s
                                text: preview.sectionNames[entry.modelData.section] ?? ""
                                font.family: Theme.type.ui
                                font.pixelSize: Theme.type.small
                                font.weight: Font.DemiBold
                                font.letterSpacing: 1.2
                                font.capitalization: Font.AllUppercase
                                color: Theme.color.inkMuted
                            }

                            Text {
                                width: parent.width
                                leftPadding: entry.modelData.depth * Theme.space.l
                                text: entry.modelData.title
                                elide: Text.ElideRight
                                font.family: Theme.type.ui
                                font.pixelSize: Theme.type.body
                                color: Theme.color.ink
                                Accessible.role: Accessible.ListItem
                                Accessible.name: text
                            }
                        }
                    }
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
