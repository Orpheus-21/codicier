pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// The goals and the progress of the writing: the session target, the goal
// and deadline of the project, the history, and the sprint timer.
T.Popup {
    id: dialog

    required property ProjectSession session
    property string goalError: ""

    function saveGoal() {
        const words = goalField.text === "" ? 0 : parseInt(goalField.text);
        goalError = session.setGoal(words, deadlineField.text) ? ""
                  : "Write the deadline as a date such as 2027-03-01.";
    }

    function longDate(iso: string): string {
        return Date.fromLocaleDateString(Qt.locale("en_GB"), iso, "yyyy-MM-dd")
            .toLocaleDateString(Qt.locale("en_GB"), "d MMMM yyyy");
    }

    implicitWidth: 600
    implicitHeight: Math.min(form.implicitHeight + topPadding + bottomPadding,
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

    component Caps: Text {
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.small
        font.weight: Font.DemiBold
        font.letterSpacing: 1.2
        font.capitalization: Font.AllUppercase
        color: Theme.color.inkMuted
    }

    component Heading: Text {
        topPadding: Theme.space.s
        font.family: Theme.type.reading
        font.pixelSize: Theme.type.large + 2
        color: Theme.color.ink
    }

    component Note: Text {
        width: parent.width
        wrapMode: Text.Wrap
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    contentItem: Flickable {
        clip: true
        contentHeight: form.implicitHeight
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: form

            width: parent.width
            spacing: Theme.space.l

            Text {
                text: "Goals and progress"
                font.family: Theme.type.reading
                font.pixelSize: Theme.type.title
                color: Theme.color.ink
            }

            Heading {
                text: "This session"
            }

            Note {
                text: "You added " + dialog.session.sessionWords.toLocaleString(Qt.locale("en_GB"), "f", 0)
                      + " words to the manuscript since the project opened."
            }

            Column {
                width: parent.width
                spacing: Theme.space.xs

                Caps {
                    text: "Word target for one session"
                }

                TextField {
                    objectName: "sessionTarget"
                    width: 160
                    Accessible.name: "Word target for one session"
                    placeholderText: "No target"
                    text: Settings.sessionTarget > 0 ? String(Settings.sessionTarget) : ""
                    validator: IntValidator {
                        bottom: 0
                        top: 100000
                    }
                    onEditingFinished: Settings.sessionTarget = text === "" ? 0 : parseInt(text)
                }
            }

            Heading {
                text: "The project"
            }

            Row {
                spacing: Theme.space.l

                Column {
                    spacing: Theme.space.xs

                    Caps {
                        text: "Word goal"
                    }

                    TextField {
                        id: goalField

                        objectName: "goalWords"
                        width: 160
                        Accessible.name: "Word goal of the project"
                        placeholderText: "No goal"
                        text: dialog.session.goalWords > 0 ? String(dialog.session.goalWords) : ""
                        validator: IntValidator {
                            bottom: 0
                            top: 100000000
                        }
                        onEditingFinished: dialog.saveGoal()
                    }
                }

                Column {
                    spacing: Theme.space.xs

                    Caps {
                        text: "Deadline"
                    }

                    TextField {
                        id: deadlineField

                        objectName: "deadline"
                        width: 160
                        Accessible.name: "Deadline of the project"
                        placeholderText: "2027-03-01"
                        text: dialog.session.deadline
                        onEditingFinished: dialog.saveGoal()
                    }
                }
            }

            Note {
                objectName: "dailyNeeded"
                text: dialog.goalError !== "" ? dialog.goalError
                    : dialog.session.dailyNeeded >= 0
                      ? "To reach " + dialog.session.goalWords.toLocaleString(Qt.locale("en_GB"), "f", 0)
                        + " words by " + dialog.longDate(dialog.session.deadline) + ", write "
                        + dialog.session.dailyNeeded.toLocaleString(Qt.locale("en_GB"), "f", 0) + " words each day."
                    : "Set a word goal and a deadline to see the words to write each day."
                color: dialog.goalError !== "" ? Theme.color.accent : Theme.color.inkMuted
            }

            Item {
                width: parent.width
                height: doneButton.height

                Button {
                    id: doneButton

                    anchors.right: parent.right
                    kind: "primary"
                    text: "Done"
                    onClicked: dialog.close()
                }
            }
        }
    }
}
