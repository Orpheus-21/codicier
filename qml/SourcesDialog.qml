pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// The bibliography of the project: the list of sources, and a form to add
// or change one. The sources live in sources.json as CSL-JSON.
T.Dialog {
    id: dialog

    title: "Sources"

    required property ProjectSession session
    property var sources: []
    // The source in the form, or null when the list shows.
    property var editing: null
    // Why the last save or removal failed, or "".
    property string error: ""

    // The CSL-JSON types that the form offers, with their names.
    readonly property var types: [["book", "Book"], ["article-journal", "Article"], ["chapter", "Chapter"],
        ["webpage", "Web page"], ["document", "Other"]]
    readonly property var fields: [["authors", "Authors", "Family, Given; Family, Given"], ["title", "Title", ""],
        ["year", "Year", "Such as 2002"], ["publisher", "Publisher", ""],
        ["container", "Journal or book", "For an article or a chapter"], ["page", "Pages", "Such as 12-19"],
        ["url", "Web address", ""], ["doi", "DOI", "Such as 10.1000/xyz"]]

    // A field of the form with its name above it.
    component LabelledField: Column {
        id: labelled

        required property var modelData
        property alias field: input

        width: (parent.width - Theme.space.l) / 2
        spacing: Theme.space.xs

        Text {
            text: labelled.modelData[1]
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.6
            color: Theme.color.inkMuted
        }

        TextField {
            id: input

            objectName: "source-" + labelled.modelData[0]
            width: parent.width
            placeholderText: labelled.modelData[2]
            Accessible.name: labelled.modelData[1]
            onAccepted: dialog.save()
        }
    }

    function refresh() {
        sources = session.sources();
    }

    function edit(source: var) {
        const form = Object.assign({ id: "", type: "book" }, source ?? {});
        editing = form;
        for (let i = 0; i < formFields.count; ++i) {
            (formFields.itemAt(i) as LabelledField).field.text = form[dialog.fields[i][0]] ?? "";
        }
        (formFields.itemAt(0) as LabelledField).field.forceActiveFocus();
    }

    function save() {
        const form = { id: editing.id, type: editing.type };
        for (let i = 0; i < formFields.count; ++i)
            form[dialog.fields[i][0]] = (formFields.itemAt(i) as LabelledField).field.text;
        if (session.saveSource(form) === "") {
            error = session.lastError;
            return;
        }
        error = "";
        editing = null;
        refresh();
    }

    implicitWidth: 620
    implicitHeight: Math.min(implicitContentHeight + topPadding + bottomPadding,
                             (parent ? parent.height : 800) - 2 * Theme.space.l)
    padding: Theme.space.xl
    modal: true
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    onAboutToShow: {
        editing = null;
        error = "";
        refresh();
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
            text: dialog.editing === null ? "Sources" : dialog.editing.id === "" ? "Add a source" : "Change the source"
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.title
            color: Theme.color.ink
        }

        // The list of sources.
        Text {
            visible: dialog.editing === null && dialog.sources.length === 0
            width: parent.width
            wrapMode: Text.Wrap
            text: "No sources yet. Add the books, articles, and pages that the work cites."
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.inkMuted
        }

        Flickable {
            visible: dialog.editing === null && dialog.sources.length > 0
            width: parent.width
            height: Math.min(contentHeight, 360)
            contentHeight: list.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: list

                width: parent.width
                spacing: Theme.space.s

                Repeater {
                    model: dialog.sources

                    Row {
                        id: row

                        required property var modelData

                        objectName: "source-" + modelData.id
                        width: list.width
                        spacing: Theme.space.s

                        Column {
                            width: row.width - edit.width - remove.width - 2 * row.spacing
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                width: parent.width
                                elide: Text.ElideRight
                                text: row.modelData.label || "Untitled source"
                                font.family: Theme.type.reading
                                font.pixelSize: Theme.type.body + 1
                                color: Theme.color.ink
                            }

                            Text {
                                text: "@" + row.modelData.id
                                font.family: Theme.type.ui
                                font.pixelSize: Theme.type.small
                                color: Theme.color.inkMuted
                            }
                        }

                        Button {
                            id: edit

                            objectName: "changeSource"
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Change"
                            Accessible.name: "Change " + (row.modelData.label || row.modelData.id)
                            onClicked: dialog.edit(row.modelData)
                        }

                        Button {
                            id: remove

                            anchors.verticalCenter: parent.verticalCenter
                            text: "Remove"
                            Accessible.name: "Remove " + (row.modelData.label || row.modelData.id)
                            onClicked: {
                                dialog.error = dialog.session.removeSource(row.modelData.id) ? "" : dialog.session.lastError;
                                dialog.refresh();
                            }
                        }
                    }
                }
            }
        }

        // The form of one source.
        Flow {
            visible: dialog.editing !== null
            width: parent.width
            spacing: Theme.space.xs

            Repeater {
                model: dialog.types

                Choice {
                    required property var modelData

                    objectName: "type-" + modelData[0]
                    label: modelData[1]
                    chosen: dialog.editing !== null && dialog.editing.type === modelData[0]
                    onClicked: dialog.editing = Object.assign({}, dialog.editing, { type: modelData[0] })
                }
            }
        }

        Grid {
            visible: dialog.editing !== null
            width: parent.width
            columns: 2
            columnSpacing: Theme.space.l
            rowSpacing: Theme.space.s

            Repeater {
                id: formFields

                model: dialog.fields

                LabelledField {}
            }
        }

        Text {
            visible: dialog.error !== ""
            width: parent.width
            wrapMode: Text.Wrap
            text: dialog.error
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            color: Theme.color.accent
        }

        Row {
            anchors.right: parent.right
            spacing: Theme.space.s

            Button {
                text: dialog.editing === null ? "Done" : "Cancel"
                onClicked: dialog.editing === null ? dialog.close() : dialog.editing = null
            }

            Button {
                objectName: dialog.editing === null ? "addSource" : "saveSource"
                kind: "primary"
                text: dialog.editing === null ? "Add a source" : "Save the source"
                onClicked: dialog.editing === null ? dialog.edit(null) : dialog.save()
            }
        }
    }
}
