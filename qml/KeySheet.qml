pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Every command of the project view and its key, in groups. The keys come
// from the key map, so a key that the author changed shows here.
T.Dialog {
    id: sheet

    title: "Keys"

    // The commands of the view: id, name, and sequence.
    required property var commands

    readonly property var groupNames: ["Write", "Find", "Plan and track", "Views and panes", "Versions", "App"]
    readonly property var groups: groupNames.map(name => ({
        name: name,
        rows: commands.filter(command => groupOf(command.id) === name)
    })).filter(group => group.rows.length > 0)

    function groupOf(id: string): string {
        if (id.startsWith("versions."))
            return "Versions";
        if (["format.bold", "format.italic", "format.highlight", "scene.new", "chapter.new", "part.new", "note.new", "page.new", "item.rename", "scene.split",
             "scene.merge", "item.trash", "snapshot.take", "comment"].includes(id))
            return "Write";
        if (["palette", "find", "find.replace", "search", "replace"].includes(id))
            return "Find";
        if (["calendar", "codex", "names", "sprint", "progress", "passes", "inbox", "changes", "sources"].includes(id))
            return "Plan and track";
        if (["help", "close"].includes(id))
            return "App";
        return "Views and panes";
    }

    implicitWidth: 760
    implicitHeight: Math.min(content.implicitHeight + topPadding + bottomPadding,
                             (parent ? parent.height : 800) - 2 * Theme.space.l)
    padding: Theme.space.xl
    modal: true
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    T.Overlay.modal: Rectangle {
        color: Qt.alpha(Theme.color.ink, 0.25)
    }

    background: Rectangle {
        radius: Theme.radius.medium
        color: Theme.color.paper
        border.width: 1
        border.color: Theme.color.rule
    }

    contentItem: Flickable {
        id: flick

        contentHeight: content.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        WheelHandler {
            onWheel: event => {
                const end = Math.max(0, flick.contentHeight - flick.height);
                flick.contentY = Math.min(end, Math.max(0, flick.contentY - event.angleDelta.y / 2));
            }
        }

        Column {
            id: content

            width: flick.width
            spacing: Theme.space.l

            Text {
                text: "Keys"
                font.family: Theme.type.reading
                font.pixelSize: Theme.type.title
                color: Theme.color.ink
            }

            Flow {
                width: parent.width
                spacing: Theme.space.xl

                Repeater {
                    model: sheet.groups

                    Column {
                        id: group

                        required property var modelData

                        width: (content.width - Theme.space.xl) / 2
                        Accessible.role: Accessible.List
                        Accessible.name: modelData.name

                        Text {
                            bottomPadding: Theme.space.xs
                            text: group.modelData.name
                            font.family: Theme.type.ui
                            font.pixelSize: Theme.type.small
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.2
                            font.capitalization: Font.AllUppercase
                            color: Theme.color.inkMuted
                        }

                        Repeater {
                            model: group.modelData.rows

                            Item {
                                id: row

                                required property var modelData

                                objectName: "keyRow"
                                width: group.width
                                height: 28
                                Accessible.role: Accessible.ListItem
                                Accessible.name: modelData.name + ", " + modelData.sequence

                                Text {
                                    anchors.left: parent.left
                                    anchors.right: key.left
                                    anchors.rightMargin: Theme.space.s
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: row.modelData.name
                                    elide: Text.ElideRight
                                    font.family: Theme.type.ui
                                    font.pixelSize: Theme.type.body
                                    color: Theme.color.ink
                                }

                                Text {
                                    id: key

                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: row.modelData.sequence
                                    font.family: Theme.type.ui
                                    font.pixelSize: Theme.type.small
                                    color: Theme.color.inkMuted
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
