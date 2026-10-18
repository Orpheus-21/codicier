pragma ComponentBehavior: Bound

import QtQuick
import Codicier

// Mock: the workspace home. Static data, no actions. For design review only.
Rectangle {
    id: root

    width: 1440
    height: 960
    color: Theme.color.paper

    readonly property var fiction: [
        { title: "The Salt Ledger", kind: "Novel", words: "61,032", goal: 90000, done: 61032, edited: "Today" },
        { title: "Winter Harbour", kind: "Novella", words: "22,410", goal: 35000, done: 22410, edited: "3 days ago" },
        { title: "The Lamplighter’s Daughter", kind: "Short story", words: "6,180", goal: 0, done: 0, edited: "2 weeks ago" }
    ]
    readonly property var nonFiction: [
        { title: "On Revision", kind: "Essay", words: "3,940", goal: 5000, done: 3940, edited: "Yesterday" },
        { title: "Notes Toward a Quiet Book", kind: "Essay collection", words: "18,200", goal: 0, done: 0, edited: "Last month" }
    ]

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

    // A thin bar that shows the words written against the goal.
    component Progress: Item {
        id: progress

        required property real fraction

        height: 2

        Rectangle {
            anchors.fill: parent
            color: Theme.color.rule
        }

        Rectangle {
            width: parent.width * Math.min(1, progress.fraction)
            height: parent.height
            color: Theme.color.accent
        }
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

    component ProjectRow: Item {
        id: row

        required property var modelData

        width: parent.width
        height: 64

        Text {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: row.modelData.title
            font.family: Theme.type.reading
            font.pixelSize: 20
            color: Theme.color.ink
        }

        Row {
            anchors.right: parent.right
            height: parent.height
            spacing: Theme.space.xl

            Label {
                anchors.verticalCenter: parent.verticalCenter
                width: 140
                text: row.modelData.kind
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                width: 110
                horizontalAlignment: Text.AlignRight
                text: row.modelData.words + " words"
            }

            Item {
                width: 96
                height: parent.height

                Progress {
                    visible: row.modelData.goal > 0
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width
                    fraction: row.modelData.goal > 0 ? row.modelData.done / row.modelData.goal : 0
                }
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                width: 100
                text: row.modelData.edited
            }

            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                iconName: "ellipsis"
            }
        }

        Rule {
            anchors.bottom: parent.bottom
            width: parent.width
        }
    }

    component Section: Column {
        id: section

        required property string heading
        required property var projects

        width: parent.width

        Caps {
            text: section.heading
            bottomPadding: Theme.space.s
        }

        Rule {
            width: parent.width
        }

        Repeater {
            model: section.projects

            ProjectRow {}
        }
    }

    // The top bar: the workspace, then search, a new project, and settings.
    Item {
        id: topBar

        width: parent.width
        height: 56

        Row {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.xl
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space.s

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "Writing Desk"
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.body
                font.weight: Font.DemiBold
                color: Theme.color.ink
            }

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: "chevron-down"
                size: 14
                color: Theme.color.inkMuted
            }
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: Theme.space.l
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space.xs

            IconButton {
                iconName: "search"
            }

            Row {
                anchors.verticalCenter: parent.verticalCenter
                leftPadding: Theme.space.s
                rightPadding: Theme.space.s
                spacing: Theme.space.xs

                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "plus"
                    size: 16
                    color: Theme.color.ink
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "New project"
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.body
                    color: Theme.color.ink
                }
            }

            IconButton {
                iconName: "settings"
            }
        }

        Rule {
            anchors.bottom: parent.bottom
            width: parent.width
        }
    }

    Column {
        id: content

        anchors.top: topBar.bottom
        anchors.topMargin: Theme.space.xxxl
        anchors.horizontalCenter: parent.horizontalCenter
        width: 880
        spacing: Theme.space.xxxl

        // Continue: the project the author wrote in last.
        Column {
            width: parent.width
            spacing: Theme.space.m

            Caps {
                text: "Continue"
            }

            Text {
                text: "The Salt Ledger"
                font.family: Theme.type.reading
                font.pixelSize: 44
                color: Theme.color.ink
            }

            Label {
                text: "Novel  ·  Draft 2  ·  Part Two, Chapter 12"
            }

            Text {
                width: parent.width
                topPadding: Theme.space.s
                bottomPadding: Theme.space.s
                text: "“You kept it,” she said to the empty room. The hand was her mother’s, quick and slanted, never patient."
                wrapMode: Text.Wrap
                font.family: Theme.type.reading
                font.italic: true
                font.pixelSize: Theme.type.readingSize
                lineHeight: Theme.type.readingLineHeight
                color: Theme.color.inkMuted
            }

            Row {
                width: parent.width
                spacing: Theme.space.l

                Progress {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - wordGoal.width - parent.spacing
                    fraction: 61032 / 90000
                }

                Label {
                    id: wordGoal

                    text: "61,032 of 90,000 words"
                }
            }

            Item {
                width: 1
                height: Theme.space.xs
            }

            Row {
                spacing: Theme.space.l

                Rectangle {
                    width: continueRow.width + 2 * Theme.space.l
                    height: 40
                    radius: Theme.radius.small
                    color: Theme.color.accent

                    Row {
                        id: continueRow

                        anchors.centerIn: parent
                        spacing: Theme.space.s

                        Icon {
                            anchors.verticalCenter: parent.verticalCenter
                            name: "pencil"
                            size: 16
                            color: Theme.color.paper
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Continue writing"
                            font.family: Theme.type.ui
                            font.pixelSize: Theme.type.body
                            font.weight: Font.Medium
                            color: Theme.color.paper
                        }
                    }
                }

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Last written today at 07:40"
                }
            }
        }

        Section {
            heading: "Fiction"
            projects: root.fiction
        }

        Section {
            heading: "Non-fiction"
            projects: root.nonFiction
        }
    }

    // The status line: totals for the day and the week.
    Item {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 40

        Rule {
            width: parent.width
        }

        Label {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.xl
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: Theme.type.small
            text: "5 projects    Today 1,284 words    This week 6,410 words"
        }
    }
}
