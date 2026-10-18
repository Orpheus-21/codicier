pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// One row of the binder: a section heading, or an item with its indent, its
// arrow, its status mark or icon, its title, and its word count.
T.ItemDelegate {
    id: row

    required property int index
    required property string itemId
    required property string title
    required property string kind
    required property int depth
    required property bool hasChildren
    required property bool expanded
    required property bool isSection
    required property string status
    required property int words
    required property bool missing
    // The view sets this for the current row.
    property bool current: false

    // The arrow was clicked.
    signal toggleRequested

    readonly property string iconName: {
        switch (row.kind) {
        case "page":
            return "file-text";
        case "note":
            return "sticky-note";
        case "file":
            return "bookmark";
        case "folder":
            return "folder";
        default:
            return "";
        }
    }

    implicitWidth: 280
    implicitHeight: isSection ? 36 : 32
    leftPadding: Theme.space.s + depth * Theme.space.l
    rightPadding: Theme.space.s
    Accessible.name: title

    contentItem: Item {
        // A section heading: small capitals with an arrow.
        Row {
            visible: row.isSection
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Theme.space.xs
            spacing: Theme.space.xs

            Text {
                text: row.title
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.small
                font.weight: Font.DemiBold
                font.letterSpacing: 1.2
                font.capitalization: Font.AllUppercase
                color: Theme.color.inkMuted
            }

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                visible: !row.expanded
                name: "chevron-right"
                size: 12
                color: Theme.color.inkMuted
            }
        }

        Row {
            visible: !row.isSection
            anchors.left: parent.left
            anchors.right: wordCount.left
            anchors.rightMargin: Theme.space.s
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space.xs

            // The arrow for a container, or the status mark for a scene.
            Item {
                width: 14
                height: 14
                anchors.verticalCenter: parent.verticalCenter

                Icon {
                    visible: row.hasChildren
                    name: row.expanded ? "chevron-down" : "chevron-right"
                    size: 14
                    color: Theme.color.inkMuted

                    TapHandler {
                        onTapped: row.toggleRequested()
                    }
                }

                // Draft: a pale ring. Revised: an accent ring. Final: a dot.
                Rectangle {
                    visible: !row.hasChildren && row.status !== ""
                    anchors.centerIn: parent
                    width: 8
                    height: 8
                    radius: 4
                    color: row.status === "final" ? Theme.color.accent : "transparent"
                    border.width: row.status === "final" ? 0 : 1.5
                    border.color: row.status === "draft" ? Theme.color.mark : Theme.color.accent
                }
            }

            Icon {
                visible: row.iconName !== ""
                anchors.verticalCenter: parent.verticalCenter
                name: row.iconName
                size: 16
                color: Theme.color.inkMuted
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 14 - parent.spacing
                       - (row.iconName !== "" ? 16 + parent.spacing : 0)
                text: row.title
                elide: Text.ElideRight
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.body
                font.weight: row.current ? Font.Medium : Font.Normal
                font.strikeout: row.missing
                color: row.missing ? Theme.color.inkMuted : Theme.color.ink
            }
        }

        Text {
            id: wordCount

            visible: !row.isSection && row.words > 0
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: Number(row.words).toLocaleString(Qt.locale("en_GB"), "f", 0)
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            color: Theme.color.inkMuted
        }
    }

    background: Rectangle {
        radius: Theme.radius.small
        color: row.current && !row.isSection ? Theme.color.selection
             : row.hovered && !row.isSection ? Qt.alpha(Theme.color.selection, 0.5) : "transparent"

        FocusRing {
            anchors.margins: 0
            visible: row.visualFocus
        }
    }
}
