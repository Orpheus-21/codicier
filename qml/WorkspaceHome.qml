pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// The workspace home: the project to continue, then all projects in rows
// under Fiction and Non-fiction. docs/design/brief.md and the T16 mock set
// the look.
Item {
    id: home

    // The workspace folder.
    property alias folder: projectList.folder
    readonly property alias projects: projectList

    // A click on a project. T28 opens it.
    signal projectActivated(string folder)

    ProjectList {
        id: projectList

        // A new project opens at once.
        onCreated: folder => home.projectActivated(folder)
    }

    function formatNumber(n: int): string {
        return Number(n).toLocaleString(Qt.locale("en_GB"), "f", 0);
    }

    component Caps: Text {
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.small
        font.weight: Font.DemiBold
        font.letterSpacing: 1.2
        font.capitalization: Font.AllUppercase
        color: Theme.color.inkMuted
    }

    component Muted: Text {
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    component Rule: Rectangle {
        height: 1
        color: Theme.color.rule
    }

    // A thin bar for the words written against the goal.
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

    component ProjectRow: T.ItemDelegate {
        id: row

        required property string title
        required property string kindLabel
        required property int words
        required property int goal
        required property string editedLabel
        required property string folder
        required property string error
        required property string author

        width: parent.width
        height: 64
        Accessible.name: row.title + (row.author !== "" ? ", by " + row.author : "") + ", " + row.kindLabel
        // Tab reaches each project, and Enter opens it, as a click does.
        focusPolicy: Qt.StrongFocus
        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()
        onClicked: home.projectActivated(row.folder)

        background: Rectangle {
            color: row.hovered ? Theme.color.surface : "transparent"

            Rule {
                anchors.bottom: parent.bottom
                width: parent.width
            }

            FocusRing {
                anchors.margins: 0
                visible: row.visualFocus
            }
        }

        contentItem: Item {
            Column {
                anchors.left: parent.left
                anchors.right: details.left
                anchors.rightMargin: Theme.space.l
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    width: parent.width
                    text: row.title
                    elide: Text.ElideRight
                    font.family: Theme.type.reading
                    font.pixelSize: Theme.type.large + 3
                    color: Theme.color.ink
                }

                Muted {
                    visible: row.author !== ""
                    width: parent.width
                    text: row.author
                    elide: Text.ElideRight
                }
            }

            Row {
                id: details

                anchors.right: parent.right
                height: parent.height
                spacing: Theme.space.xl

                Muted {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: row.error !== ""
                    width: 400
                    elide: Text.ElideRight
                    text: row.error
                    color: Theme.color.accent
                }

                Muted {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: row.error === ""
                    width: 140
                    text: row.kindLabel
                }

                Muted {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: row.error === ""
                    width: 110
                    horizontalAlignment: Text.AlignRight
                    text: home.formatNumber(row.words) + " words"
                }

                Item {
                    width: 96
                    height: parent.height
                    visible: row.error === ""

                    Progress {
                        visible: row.goal > 0
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        fraction: row.goal > 0 ? row.words / row.goal : 0
                    }
                }

                Muted {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 100
                    text: row.editedLabel
                }

                IconButton {
                    objectName: "deleteProject"
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "trash"
                    label: "Delete " + (row.title || row.folder)
                    onClicked: deleteDialog.ask(row.folder, row.title || row.folder.split("/").pop())
                }
            }
        }
    }

    component Section: Column {
        id: section

        required property string heading
        // Fiction rows, or non-fiction rows. A project that did not load is
        // in the non-fiction list, because its kind is unknown.
        required property bool fiction
        required property int count

        width: parent.width
        visible: count > 0

        Caps {
            text: section.heading
            bottomPadding: Theme.space.s
        }

        Rule {
            width: parent.width
        }

        // The rows of the other section are hidden, and a hidden row takes no
        // space in a Column.
        Repeater {
            model: projectList

            ProjectRow {
                required property bool fiction

                visible: fiction === section.fiction
            }
        }
    }

    Flickable {
        anchors.fill: parent
        anchors.bottomMargin: statusLine.height
        contentHeight: content.height + 2 * Theme.space.xxxl
        clip: true

        Column {
            id: content

            y: Theme.space.xxxl
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(880, parent.width - 2 * Theme.space.xl)
            spacing: Theme.space.xxxl

            // The project with the newest edit.
            Column {
                id: continueBlock

                readonly property var latest: projectList.count > 0 ? projectList.get(0) : ({})

                visible: projectList.count > 0 && latest.error === ""
                width: parent.width
                spacing: Theme.space.m

                Caps {
                    text: "Continue"
                }

                Text {
                    width: parent.width
                    text: continueBlock.latest.title ?? ""
                    elide: Text.ElideRight
                    font.family: Theme.type.reading
                    font.pixelSize: 44
                    color: Theme.color.ink
                }

                Muted {
                    text: (continueBlock.latest.kindLabel ?? "")
                          + (continueBlock.latest.author ? " by " + continueBlock.latest.author : "")
                }

                Text {
                    visible: text !== ""
                    width: parent.width
                    topPadding: Theme.space.s
                    bottomPadding: Theme.space.s
                    text: continueBlock.latest.lastLine ?? ""
                    wrapMode: Text.Wrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    font.family: Theme.type.reading
                    font.italic: true
                    font.pixelSize: Theme.type.readingSize
                    lineHeightMode: Text.FixedHeight
                    lineHeight: Theme.type.readingLine
                    color: Theme.color.inkMuted
                }

                Row {
                    visible: (continueBlock.latest.goal ?? 0) > 0
                    width: parent.width
                    spacing: Theme.space.l

                    Progress {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - goalText.width - parent.spacing
                        fraction: (continueBlock.latest.goal ?? 0) > 0
                                  ? continueBlock.latest.words / continueBlock.latest.goal : 0
                    }

                    Muted {
                        id: goalText

                        text: home.formatNumber(continueBlock.latest.words ?? 0) + " of "
                              + home.formatNumber(continueBlock.latest.goal ?? 0) + " words"
                    }
                }

                Row {
                    spacing: Theme.space.l

                    Button {
                        kind: "primary"
                        iconName: "pencil"
                        text: "Continue writing"
                        onClicked: home.projectActivated(continueBlock.latest.folder)
                    }

                    Muted {
                        anchors.verticalCenter: parent.verticalCenter
                        // A project with no words has not been written yet.
                        text: continueBlock.latest.edited && (continueBlock.latest.words ?? 0) > 0
                              ? "Last written " + (continueBlock.latest.editedLabel === "Today" ? "today"
                                                   : continueBlock.latest.editedLabel.toLowerCase())
                                + " at " + Qt.formatTime(continueBlock.latest.edited, "HH:mm")
                              : ""
                    }
                }
            }

            // An empty workspace.
            Column {
                visible: projectList.count === 0
                width: parent.width
                spacing: Theme.space.l

                Text {
                    text: "No projects yet"
                    font.family: Theme.type.reading
                    font.pixelSize: Theme.type.title
                    color: Theme.color.ink
                }

                Muted {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: "A project is a folder in this workspace. Make the first one from a template."
                }

                Button {
                    kind: "primary"
                    iconName: "plus"
                    text: "New project"
                    onClicked: newProject.open()
                }
            }

            Section {
                heading: "Fiction"
                fiction: true
                count: projectList.fictionCount
            }

            Section {
                heading: "Non-fiction"
                fiction: false
                count: projectList.nonFictionCount
            }
        }
    }

    // The status line.
    Item {
        id: statusLine

        anchors.bottom: parent.bottom
        width: parent.width
        height: 40

        Rule {
            width: parent.width
        }

        Muted {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.xl
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: Theme.type.small
            text: projectList.count + (projectList.count === 1 ? " project" : " projects") + "    "
                  + home.formatNumber(projectList.totalWords) + " words in all"
        }
    }

    function openNewProject() {
        newProject.open();
    }

    DeleteProjectDialog {
        id: deleteDialog

        anchors.centerIn: parent
        projects: projectList
    }

    NewProjectDialog {
        id: newProject

        anchors.centerIn: parent
        projects: projectList
    }
}
