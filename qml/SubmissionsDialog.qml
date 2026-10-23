pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// The submissions of the work: where each piece went, when, and the
// result. The open ones show first; "All" shows the closed ones too.
T.Dialog {
    id: dialog

    title: "Submissions"

    required property ProjectSession session
    property var submissions: []
    property bool all: false
    // The piece of the next submission: an item id, or "" for the project.
    property string piece: ""
    readonly property var shown: submissions.filter(each => all || each.result === "")
        .sort((a, b) => (a.result === "" ? 0 : 1) - (b.result === "" ? 0 : 1) || b.sent.localeCompare(a.sent))
    // The kinds of items that go out as a piece.
    readonly property var pieces: ["part", "chapter", "scene", "folder"]
    readonly property var results: [["", "Open"], ["accepted", "Accepted"], ["rejected", "Rejected"],
        ["withdrawn", "Withdrawn"]]

    function refresh() {
        submissions = session.submissions();
    }

    function add() {
        if (!session.addSubmission(piece, venue.text, sent.text))
            return;
        venue.text = "";
        refresh();
    }

    implicitWidth: 640
    implicitHeight: Math.min(implicitContentHeight + topPadding + bottomPadding,
                             (parent ? parent.height : 800) - 2 * Theme.space.l)
    padding: Theme.space.xl
    modal: true
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    onAboutToShow: {
        // The open text is the likely piece, else the whole project.
        piece = pieces.includes(session.fields(session.currentId).kind) ? session.currentId : "";
        sent.text = Qt.formatDate(new Date(), "yyyy-MM-dd");
        refresh();
        venue.forceActiveFocus();
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

        Row {
            width: parent.width
            spacing: Theme.space.l

            Text {
                text: "Submissions"
                font.family: Theme.type.reading
                font.pixelSize: Theme.type.title
                color: Theme.color.ink
            }

            Choice {
                objectName: "openOnly"
                anchors.verticalCenter: parent.verticalCenter
                label: "Open"
                chosen: !dialog.all
                onClicked: dialog.all = false
            }

            Choice {
                objectName: "allSubmissions"
                anchors.verticalCenter: parent.verticalCenter
                label: "All"
                chosen: dialog.all
                onClicked: dialog.all = true
            }
        }

        Text {
            visible: dialog.shown.length === 0
            width: parent.width
            wrapMode: Text.Wrap
            text: dialog.all ? "No submissions yet." : "No open submissions."
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.inkMuted
        }

        Flickable {
            visible: dialog.shown.length > 0
            width: parent.width
            height: Math.min(contentHeight, 320)
            contentHeight: list.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: list

                width: parent.width
                spacing: Theme.space.m

                Repeater {
                    model: dialog.shown

                    Column {
                        id: row

                        required property var modelData

                        objectName: "submission-" + modelData.index
                        width: list.width
                        spacing: Theme.space.xs

                        Text {
                            width: parent.width
                            elide: Text.ElideRight
                            text: row.modelData.title + " to " + row.modelData.venue
                            font.family: Theme.type.reading
                            font.pixelSize: Theme.type.body + 1
                            color: Theme.color.ink
                        }

                        Row {
                            spacing: Theme.space.xs

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                rightPadding: Theme.space.s
                                text: "Sent " + row.modelData.sent
                                font.family: Theme.type.ui
                                font.pixelSize: Theme.type.small
                                color: Theme.color.inkMuted
                            }

                            Repeater {
                                model: dialog.results

                                Choice {
                                    required property var modelData

                                    objectName: "result-" + (modelData[0] || "open")
                                    label: modelData[1]
                                    chosen: row.modelData.result === modelData[0]
                                    onClicked: {
                                        dialog.session.setSubmissionResult(row.modelData.index, modelData[0]);
                                        dialog.refresh();
                                    }
                                }
                            }

                            IconButton {
                                iconName: "x"
                                label: "Remove the submission"
                                onClicked: {
                                    dialog.session.removeSubmission(row.modelData.index);
                                    dialog.refresh();
                                }
                            }
                        }
                    }
                }
            }
        }

        // A new submission: the piece, the venue, and the date it went.
        Rectangle {
            width: parent.width
            height: 1
            color: Theme.color.rule
        }

        Row {
            width: parent.width
            spacing: Theme.space.s

            Button {
                id: pieceButton

                objectName: "submissionPiece"
                width: (parent.width - 2 * parent.spacing) * 0.36
                text: dialog.piece === "" ? dialog.session.title
                    : (dialog.session.fields(dialog.piece).title ?? dialog.session.title)
                iconName: "chevron-down"
                iconAfterText: true
                Accessible.name: "Piece: " + text
                onClicked: pieceMenu.popup(pieceButton, 0, pieceButton.height)
            }

            TextField {
                id: venue

                objectName: "submissionVenue"
                width: (parent.width - 2 * parent.spacing) * 0.40
                placeholderText: "Where it went, such as a magazine"
                Accessible.name: "Venue"
                onAccepted: dialog.add()
            }

            TextField {
                id: sent

                objectName: "submissionSent"
                width: (parent.width - 2 * parent.spacing) * 0.24
                placeholderText: "yyyy-mm-dd"
                Accessible.name: "Date sent"
                onAccepted: dialog.add()
            }
        }

        Menu {
            id: pieceMenu

            T.Action {
                text: "The whole project"
                onTriggered: dialog.piece = ""
            }

            Instantiator {
                model: dialog.session.items().filter(item => dialog.pieces.includes(item.kind))

                delegate: T.Action {
                    required property var modelData

                    text: modelData.title
                    onTriggered: dialog.piece = modelData.id
                }

                onObjectAdded: (index, action) => pieceMenu.insertAction(index + 1, action as T.Action)
                onObjectRemoved: (index, action) => pieceMenu.removeAction(action as T.Action)
            }
        }

        Row {
            anchors.right: parent.right
            spacing: Theme.space.s

            Button {
                text: "Done"
                onClicked: dialog.close()
            }

            Button {
                objectName: "addSubmission"
                kind: "primary"
                text: "Add the submission"
                enabled: venue.text.trim() !== ""
                onClicked: dialog.add()
            }
        }
    }
}
