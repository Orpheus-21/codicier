pragma ComponentBehavior: Bound

import QtQuick
import Codicier

// Mock: the binder with every row state, and the inspector for the current
// scene. Static data, no actions. For design review only.
Rectangle {
    id: root

    width: 1440
    height: 960
    color: Theme.color.paper

    component Label: Text {
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    component Caps: Label {
        font.pixelSize: Theme.type.small
        font.weight: Font.DemiBold
        font.letterSpacing: 1.2
        font.capitalization: Font.AllUppercase
    }

    component Rule: Rectangle {
        height: 1
        color: Theme.color.rule
    }

    component IconButton: Item {
        id: button

        required property string iconName

        width: Theme.space.xxl
        height: Theme.space.xxl

        Icon {
            anchors.centerIn: parent
            name: button.iconName
            size: 18
            color: Theme.color.inkMuted
        }
    }

    // The scene status as a shape, so the theme needs no extra colours:
    // "draft" is an empty pale ring, "revised" an accent ring, "final" a dot.
    component StatusMark: Rectangle {
        id: statusMark

        required property string status

        width: 8
        height: 8
        radius: 4
        color: status === "final" ? Theme.color.accent : "transparent"
        border.width: status === "final" ? 0 : 1.5
        border.color: status === "draft" ? Theme.color.mark : Theme.color.accent
    }

    // One row of the binder. depth sets the indent.
    component BinderRow: Rectangle {
        id: binderRow

        required property string label
        property int depth: 0
        property string iconName: ""
        property string status: ""
        property string words: ""
        property bool open: false
        property bool hasChildren: false
        property bool selected: false
        property bool dragged: false

        width: parent.width
        height: 32
        radius: Theme.radius.small
        color: selected ? Theme.color.selection : "transparent"
        opacity: dragged ? 0.45 : 1

        Row {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.s + binderRow.depth * Theme.space.l
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space.xs

            Item {
                width: 14
                height: 14
                anchors.verticalCenter: parent.verticalCenter

                Icon {
                    visible: binderRow.hasChildren
                    name: binderRow.open ? "chevron-down" : "chevron-right"
                    size: 14
                    color: Theme.color.inkMuted
                }

                StatusMark {
                    visible: binderRow.status !== ""
                    anchors.centerIn: parent
                    status: binderRow.status
                }
            }

            Icon {
                visible: binderRow.iconName !== ""
                anchors.verticalCenter: parent.verticalCenter
                name: binderRow.iconName
                size: 16
                color: Theme.color.inkMuted
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: binderRow.width - binderRow.depth * Theme.space.l - 3 * Theme.space.xl
                       - (binderRow.iconName !== "" ? 20 : 0)
                elide: Text.ElideRight
                text: binderRow.label
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.body
                font.weight: binderRow.selected ? Font.Medium : Font.Normal
                color: Theme.color.ink
            }
        }

        Label {
            anchors.right: parent.right
            anchors.rightMargin: Theme.space.s
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: Theme.type.small
            text: binderRow.words
        }
    }

    // The line that shows where a dragged row will land.
    component DropLine: Item {
        property int depth: 0

        width: parent.width
        height: 2

        Rectangle {
            x: Theme.space.s + parent.depth * Theme.space.l + 14
            width: parent.width - x
            height: 2
            radius: 1
            color: Theme.color.accent
        }
    }

    // One field of the inspector: a small label above the value.
    component Field: Column {
        id: field

        required property string label
        required property string value

        width: parent.width
        spacing: Theme.space.xs

        Caps {
            text: field.label
        }

        Text {
            width: parent.width
            text: field.value
            wrapMode: Text.Wrap
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.ink
        }
    }

    component Tag: Rectangle {
        id: tag

        required property string label

        width: tagText.width + 2 * Theme.space.s
        height: 26
        radius: Theme.radius.small
        color: "transparent"
        border.width: 1
        border.color: Theme.color.rule

        Label {
            id: tagText

            anchors.centerIn: parent
            font.pixelSize: Theme.type.small
            text: tag.label
        }
    }

    // The binder.
    Rectangle {
        id: binder

        width: 320
        height: parent.height - statusBar.height
        color: Theme.color.surface

        Column {
            x: Theme.space.m
            y: Theme.space.l
            width: parent.width - 2 * Theme.space.m
            spacing: 2

            Row {
                leftPadding: Theme.space.s
                spacing: Theme.space.s

                Text {
                    text: "The Salt Ledger"
                    font.family: Theme.type.reading
                    font.pixelSize: Theme.type.large + 3
                    color: Theme.color.ink
                }

                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "chevron-down"
                    size: 14
                    color: Theme.color.inkMuted
                }
            }

            Item {
                width: 1
                height: Theme.space.m
            }

            // The filter field.
            Rectangle {
                width: parent.width
                height: 34
                radius: Theme.radius.small
                color: Theme.color.paper
                border.width: 1
                border.color: Theme.color.rule

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space.s
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space.s

                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: "search"
                        size: 15
                        color: Theme.color.inkMuted
                    }

                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Filter the binder"
                    }
                }
            }

            Item {
                width: 1
                height: Theme.space.m
            }

            Caps {
                leftPadding: Theme.space.s
                bottomPadding: Theme.space.xs
                text: "Front matter"
            }

            BinderRow { label: "Title page"; iconName: "file-text" }
            BinderRow { label: "Dedication"; iconName: "file-text" }

            Item {
                width: 1
                height: Theme.space.m
            }

            Caps {
                leftPadding: Theme.space.s
                bottomPadding: Theme.space.xs
                text: "Manuscript"
            }

            BinderRow { label: "Part One"; hasChildren: true; words: "24,310" }
            BinderRow { label: "Part Two"; hasChildren: true; open: true; words: "26,842" }
            BinderRow { label: "12  The March Accounts"; depth: 1; hasChildren: true; open: true; words: "4,960" }
            BinderRow { label: "The River Office"; depth: 2; status: "final"; words: "1,630" }
            BinderRow { label: "A Date She Did Not Know"; depth: 2; status: "revised"; words: "1,284"; selected: true }
            BinderRow { label: "Rosemary Again"; depth: 2; status: "draft"; words: "2,046"; dragged: true }
            BinderRow { label: "13  High Water"; depth: 1; hasChildren: true; open: true; words: "3,880" }
            BinderRow { label: "The Flood Gate"; depth: 2; status: "draft"; words: "2,210" }

            DropLine {
                depth: 2
            }

            BinderRow { label: "Night at the Lock"; depth: 2; status: "draft"; words: "1,670" }
            BinderRow { label: "Part Three"; hasChildren: true; words: "9,880" }

            Item {
                width: 1
                height: Theme.space.m
            }

            Caps {
                leftPadding: Theme.space.s
                bottomPadding: Theme.space.xs
                text: "Back matter"
            }

            BinderRow { label: "Acknowledgements"; iconName: "file-text" }
        }

        BinderRow {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Theme.space.m
            x: Theme.space.m
            width: parent.width - 2 * Theme.space.m
            label: "Trash"
            iconName: "trash"
            words: "3"
        }

        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: Theme.color.rule
        }
    }

    // The page, narrower because the inspector is open.
    Item {
        anchors.left: binder.right
        anchors.right: inspector.left
        height: parent.height - statusBar.height

        Item {
            id: editorBar

            width: parent.width
            height: 48

            Label {
                anchors.left: parent.left
                anchors.leftMargin: Theme.space.xl
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: Theme.type.small + 1
                text: "Part Two   /   12  The March Accounts   /   A Date She Did Not Know"
            }
        }

        Text {
            anchors.top: editorBar.bottom
            anchors.topMargin: Theme.space.xxxl
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width - 2 * Theme.space.xxl, 560)
            text: "The letter said the fourth of March, and nothing else on the first line. Mara read it twice, then held the paper to the window, as if the light could find the year that the ink had left out.\n\n“She never wrote a date without a reason,” said Tomas."
            wrapMode: Text.Wrap
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.readingSize
            lineHeightMode: Text.FixedHeight
            lineHeight: Theme.type.readingLine
            color: Theme.color.ink
        }
    }

    // The inspector for the current scene.
    Rectangle {
        id: inspector

        anchors.right: parent.right
        width: 340
        height: parent.height - statusBar.height
        color: Theme.color.surface

        Rectangle {
            width: 1
            height: parent.height
            color: Theme.color.rule
        }

        Column {
            x: Theme.space.xl
            y: Theme.space.l
            width: parent.width - 2 * Theme.space.xl
            spacing: Theme.space.l

            Item {
                width: parent.width
                height: Theme.space.xxl

                Caps {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Scene"
                }

                IconButton {
                    anchors.right: parent.right
                    anchors.rightMargin: -Theme.space.s
                    iconName: "x"
                }
            }

            Text {
                width: parent.width
                text: "A Date She Did Not Know"
                wrapMode: Text.Wrap
                font.family: Theme.type.reading
                font.pixelSize: Theme.type.title
                color: Theme.color.ink
            }

            Text {
                width: parent.width
                text: "Mara finds a date in her mother’s letter. Tomas says the date is a reason. The March accounts match it."
                wrapMode: Text.Wrap
                font.family: Theme.type.reading
                font.italic: true
                font.pixelSize: Theme.type.large
                lineHeightMode: Text.FixedHeight
                lineHeight: Math.round(Theme.type.large * 1.45)
                color: Theme.color.inkMuted
            }

            Rule {
                width: parent.width
            }

            Grid {
                width: parent.width
                columns: 2
                columnSpacing: Theme.space.l
                rowSpacing: Theme.space.l

                Column {
                    width: (parent.width - parent.columnSpacing) / 2
                    spacing: Theme.space.xs

                    Caps {
                        text: "Status"
                    }

                    Row {
                        spacing: Theme.space.s

                        StatusMark {
                            anchors.verticalCenter: parent.verticalCenter
                            status: "revised"
                        }

                        Text {
                            text: "Revised"
                            font.family: Theme.type.ui
                            font.pixelSize: Theme.type.body
                            color: Theme.color.ink
                        }
                    }
                }

                Field {
                    width: (parent.width - parent.columnSpacing) / 2
                    label: "Point of view"
                    value: "Mara"
                }

                Field {
                    width: (parent.width - parent.columnSpacing) / 2
                    label: "Place"
                    value: "The river office"
                }

                Field {
                    width: (parent.width - parent.columnSpacing) / 2
                    label: "Time"
                    value: "4 March, evening"
                }
            }

            Column {
                width: parent.width
                spacing: Theme.space.s

                Item {
                    width: parent.width
                    height: wordsLabel.height

                    Caps {
                        id: wordsLabel

                        text: "Words"
                    }

                    Label {
                        anchors.right: parent.right
                        font.pixelSize: Theme.type.small
                        text: "1,284 of 1,500"
                    }
                }

                Item {
                    width: parent.width
                    height: 2

                    Rectangle {
                        anchors.fill: parent
                        color: Theme.color.rule
                    }

                    Rectangle {
                        width: parent.width * 1284 / 1500
                        height: parent.height
                        color: Theme.color.accent
                    }
                }
            }

            Column {
                width: parent.width
                spacing: Theme.space.s

                Caps {
                    text: "Tags"
                }

                Flow {
                    width: parent.width
                    spacing: Theme.space.s

                    Tag { label: "letter" }
                    Tag { label: "mother" }
                    Tag { label: "accounts" }
                    Tag { label: "clue" }
                }
            }

            Rule {
                width: parent.width
            }

            Column {
                width: parent.width
                spacing: Theme.space.s

                Caps {
                    text: "Notes"
                }

                Text {
                    width: parent.width
                    text: "The figure in the column is a count of days, not money. Do not say so yet. Let Tomas almost see it in chapter 13."
                    wrapMode: Text.Wrap
                    font.family: Theme.type.reading
                    font.pixelSize: Theme.type.large
                    lineHeightMode: Text.FixedHeight
                lineHeight: Math.round(Theme.type.large * 1.45)
                    color: Theme.color.ink
                }
            }
        }
    }

    Item {
        id: statusBar

        anchors.bottom: parent.bottom
        width: parent.width
        height: 36

        Rule {
            width: parent.width
        }

        Label {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.xl
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: Theme.type.small
            text: "Scene 1,284 words    Chapter 4,960    Book 61,032 of 90,000"
        }
    }
}
