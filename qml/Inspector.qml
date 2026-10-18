pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// The inspector for the selected item, as in the T18 mock: the synopsis,
// the status, the point of view, the place, the time, the word target, the
// tags, and the notes. A field saves when the author leaves it or presses
// Enter, and the values go to binder.toml.
Pane {
    id: inspector

    required property ProjectSession session
    // The fields of the selected item, read again after each change.
    property var fields: ({})

    function refresh() {
        inspector.fields = session.fields(session.currentId);
    }

    // Saves one field if its value changed. Returns true if it saved.
    function save(key: string, value: var): bool {
        const id = session.currentId;
        if (id === "" || JSON.stringify(inspector.fields[key]) === JSON.stringify(value))
            return false;
        return session.setField(id, key, value);
    }

    objectName: "inspector"
    padding: Theme.space.xl
    Component.onCompleted: refresh()

    Connections {
        target: inspector.session

        function onCurrentIdChanged() {
            inspector.refresh();
        }

        function onEdited() {
            inspector.refresh();
        }

        function onLoaded() {
            inspector.refresh();
        }
    }

    component Caps: Text {
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.small
        font.weight: Font.DemiBold
        font.letterSpacing: 1.2
        font.capitalization: Font.AllUppercase
        color: Theme.color.inkMuted
    }

    component Rule: Rectangle {
        height: 1
        color: Theme.color.rule
    }

    // A labelled one-line field that saves on Enter or when it loses focus.
    component Field: Column {
        id: field

        required property string label
        required property string key

        spacing: Theme.space.xs

        Caps {
            text: field.label
        }

        TextField {
            width: parent.width
            text: inspector.fields[field.key] ?? ""
            onEditingFinished: inspector.save(field.key, text)
        }
    }

    // One choice of a set, such as "Left out".
    component Choice: T.ItemDelegate {
        id: option

        required property string label
        required property bool chosen

        implicitWidth: optionText.implicitWidth + 2 * Theme.space.s
        implicitHeight: 30

        background: Rectangle {
            radius: Theme.radius.small
            color: option.chosen ? Theme.color.selection
                 : option.hovered ? Qt.alpha(Theme.color.selection, 0.5) : "transparent"
        }

        contentItem: Item {
            Text {
                id: optionText

                anchors.centerIn: parent
                text: option.label
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.small + 1
                color: Theme.color.ink
            }
        }
    }

    // The status as the shape from the binder: a pale ring for draft, an
    // accent ring for revised, and an accent dot for final.
    component StatusChoice: T.ItemDelegate {
        id: choice

        required property string status
        required property string label
        readonly property bool chosen: inspector.fields.status === status

        implicitWidth: row.implicitWidth + 2 * Theme.space.s
        implicitHeight: 30
        onClicked: inspector.save("status", choice.status)

        background: Rectangle {
            radius: Theme.radius.small
            color: choice.chosen ? Theme.color.selection
                 : choice.hovered ? Qt.alpha(Theme.color.selection, 0.5) : "transparent"
        }

        contentItem: Item {
            Row {
                id: row

                anchors.centerIn: parent
                spacing: Theme.space.xs

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 8
                    height: 8
                    radius: 4
                    color: choice.status === "final" ? Theme.color.accent : "transparent"
                    border.width: choice.status === "final" ? 0 : 1.5
                    border.color: choice.status === "draft" ? Theme.color.mark : Theme.color.accent
                }

                Text {
                    text: choice.label
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.small + 1
                    color: Theme.color.ink
                }
            }
        }
    }

    Text {
        visible: inspector.session.currentId === ""
        anchors.centerIn: parent
        text: "Choose an item in the binder."
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    Flickable {
        visible: inspector.session.currentId !== ""
        anchors.fill: parent
        contentHeight: column.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: column

            width: parent.width
            spacing: Theme.space.l

            Caps {
                text: inspector.fields.kind ?? ""
            }

            Text {
                width: parent.width
                text: inspector.fields.title ?? ""
                wrapMode: Text.Wrap
                font.family: Theme.type.reading
                font.pixelSize: Theme.type.title
                color: Theme.color.ink
            }

            Column {
                width: parent.width
                spacing: Theme.space.xs

                Caps {
                    text: "Synopsis"
                }

                TextArea {
                    width: parent.width
                    text: inspector.fields.synopsis ?? ""
                    placeholderText: "What happens in this scene"
                    font.family: Theme.type.reading
                    font.italic: true
                    font.pixelSize: Theme.type.large - 1
                    onEditingFinished: inspector.save("synopsis", text)
                }
            }

            Rule {
                width: parent.width
            }

            Column {
                visible: inspector.fields.kind === "scene"
                width: parent.width
                spacing: Theme.space.xs

                Caps {
                    text: "Status"
                }

                Row {
                    spacing: Theme.space.xs

                    StatusChoice {
                        status: "draft"
                        label: "Draft"
                    }

                    StatusChoice {
                        status: "revised"
                        label: "Revised"
                    }

                    StatusChoice {
                        status: "final"
                        label: "Final"
                    }
                }
            }

            Column {
                visible: inspector.fields.kind === "scene"
                width: parent.width
                spacing: Theme.space.xs

                Caps {
                    text: "Manuscript"
                }

                Row {
                    spacing: Theme.space.xs

                    Choice {
                        label: "In the manuscript"
                        chosen: inspector.fields.include ?? true
                        onClicked: inspector.save("include", true)
                    }

                    Choice {
                        label: "Left out"
                        chosen: !(inspector.fields.include ?? true)
                        onClicked: inspector.save("include", false)
                    }
                }
            }

            Field {
                visible: inspector.fields.kind === "scene"
                width: parent.width
                label: "Point of view"
                key: "pov"
            }

            Field {
                visible: inspector.fields.kind === "scene"
                width: parent.width
                label: "Place"
                key: "place"
            }

            Field {
                visible: inspector.fields.kind === "scene"
                width: parent.width
                label: "Time"
                key: "time"
            }

            Column {
                width: parent.width
                spacing: Theme.space.xs

                Item {
                    width: parent.width
                    height: targetLabel.height

                    Caps {
                        id: targetLabel

                        text: "Word target"
                    }

                    Text {
                        anchors.right: parent.right
                        text: Number(inspector.fields.words ?? 0).toLocaleString(Qt.locale("en_GB"), "f", 0)
                              + ((inspector.fields.target ?? 0) > 0
                                 ? " of " + Number(inspector.fields.target).toLocaleString(Qt.locale("en_GB"), "f", 0)
                                 : " words")
                        font.family: Theme.type.ui
                        font.pixelSize: Theme.type.small
                        color: Theme.color.inkMuted
                    }
                }

                Item {
                    visible: (inspector.fields.target ?? 0) > 0
                    width: parent.width
                    height: 2

                    Rectangle {
                        anchors.fill: parent
                        color: Theme.color.rule
                    }

                    Rectangle {
                        width: parent.width * Math.min(1, (inspector.fields.words ?? 0) / Math.max(1, inspector.fields.target ?? 1))
                        height: parent.height
                        color: Theme.color.accent
                    }
                }

                TextField {
                    width: parent.width
                    placeholderText: "No target"
                    text: (inspector.fields.target ?? 0) > 0 ? String(inspector.fields.target) : ""
                    validator: IntValidator {
                        bottom: 0
                        top: 10000000
                    }
                    onEditingFinished: inspector.save("target", text === "" ? 0 : parseInt(text))
                }
            }

            Column {
                width: parent.width
                spacing: Theme.space.xs

                Caps {
                    text: "Tags"
                }

                TextField {
                    width: parent.width
                    placeholderText: "Tags, split by commas"
                    text: (inspector.fields.tags ?? []).join(", ")
                    onEditingFinished: inspector.save("tags", text.split(",").map(t => t.trim()).filter(t => t !== ""))
                }
            }

            Rule {
                width: parent.width
            }

            // Snapshots: copies of the text at a moment, newest first.
            Column {
                id: snapshotSection

                property var list: []

                function refresh() {
                    snapshotSection.list = inspector.session.snapshots(inspector.session.currentId);
                }

                visible: inspector.fields.kind === "scene" || inspector.fields.kind === "page"
                         || inspector.fields.kind === "note"
                width: parent.width
                spacing: Theme.space.s
                Component.onCompleted: refresh()

                Connections {
                    target: inspector.session

                    function onSnapshotsChanged() {
                        snapshotSection.refresh();
                    }

                    function onCurrentIdChanged() {
                        snapshotSection.refresh();
                    }
                }

                Caps {
                    text: "Snapshots"
                }

                Row {
                    width: parent.width
                    spacing: Theme.space.s

                    TextField {
                        id: snapshotTitle

                        width: parent.width - takeButton.width - parent.spacing
                        placeholderText: "Title (optional)"
                        onAccepted: takeButton.clicked()
                    }

                    Button {
                        id: takeButton

                        text: "Take"
                        onClicked: {
                            inspector.session.takeSnapshot(inspector.session.currentId, snapshotTitle.text);
                            snapshotTitle.text = "";
                        }
                    }
                }

                Repeater {
                    model: snapshotSection.list

                    Item {
                        id: snapshotRow

                        required property var modelData

                        width: snapshotSection.width
                        height: 40

                        Column {
                            anchors.left: parent.left
                            anchors.right: restoreButton.left
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                width: parent.width
                                text: snapshotRow.modelData.title !== "" ? snapshotRow.modelData.title
                                                                         : snapshotRow.modelData.label
                                elide: Text.ElideRight
                                font.family: Theme.type.ui
                                font.pixelSize: Theme.type.body
                                color: Theme.color.ink
                            }

                            Text {
                                visible: snapshotRow.modelData.title !== ""
                                text: snapshotRow.modelData.label
                                font.family: Theme.type.ui
                                font.pixelSize: Theme.type.small
                                color: Theme.color.inkMuted
                            }
                        }

                        Button {
                            id: restoreButton

                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Restore"
                            onClicked: inspector.session.restoreSnapshot(inspector.session.currentId,
                                                                         snapshotRow.modelData.path)
                        }
                    }
                }
            }

            Rule {
                width: parent.width
            }

            Column {
                width: parent.width
                spacing: Theme.space.xs

                Caps {
                    text: "Notes"
                }

                TextArea {
                    width: parent.width
                    text: inspector.fields.notes ?? ""
                    placeholderText: "Notes for this item"
                    font.family: Theme.type.reading
                    font.pixelSize: Theme.type.large - 1
                    onEditingFinished: inspector.save("notes", text)
                }
            }
        }
    }
}
