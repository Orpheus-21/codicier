pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// The goals and the progress of the writing: the session target, the goal
// and deadline of the project, the history, and the sprint timer.
T.Popup {
    id: dialog

    required property ProjectSession session
    // True while a sprint runs. The view owns the timer.
    property bool sprintRunning: false
    signal sprintRequested
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
                text: "Today"
            }

            Note {
                objectName: "today"
                text: "You wrote " + dialog.session.todayWords.toLocaleString(Qt.locale("en_GB"), "f", 0)
                      + (dialog.session.dailyGoal > 0
                         ? " of " + dialog.session.dailyGoal.toLocaleString(Qt.locale("en_GB"), "f", 0) : "")
                      + " words today"
                      + (dialog.session.todayMinutes > 0
                         ? " in " + dialog.session.todayMinutes + (dialog.session.todayMinutes === 1 ? " minute" : " minutes")
                         : "")
                      + ". "
                      + (dialog.session.streak === 1 ? "1 day in a row." : dialog.session.streak + " days in a row.")
            }

            Column {
                width: parent.width
                spacing: Theme.space.xs

                Caps {
                    text: "Words each day"
                }

                TextField {
                    objectName: "dailyGoal"
                    width: 160
                    Accessible.name: "Word goal for each day"
                    placeholderText: "No goal"
                    text: dialog.session.dailyGoal > 0 ? String(dialog.session.dailyGoal) : ""
                    validator: IntValidator {
                        bottom: 0
                        top: 100000
                    }
                    onEditingFinished: dialog.session.setDailyGoal(text === "" ? 0 : parseInt(text))
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

            Heading {
                text: "Sprint"
            }

            Row {
                spacing: Theme.space.s

                TextField {
                    objectName: "sprintMinutes"
                    anchors.verticalCenter: parent.verticalCenter
                    width: 80
                    Accessible.name: "Minutes of a sprint"
                    text: String(Settings.sprintMinutes)
                    validator: IntValidator {
                        bottom: 1
                        top: 240
                    }
                    onEditingFinished: Settings.sprintMinutes = parseInt(text)
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "minutes"
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.body
                    color: Theme.color.inkMuted
                }

                Button {
                    objectName: "sprintButton"
                    text: dialog.sprintRunning ? "Stop the sprint" : "Start a sprint"
                    onClicked: dialog.sprintRequested()
                }
            }

            Heading {
                text: "History"
            }

            // The history with labels: the weekday of each row on the left and
            // the month where a column starts one on top.
            Row {
                spacing: Theme.space.s

                Column {
                    // Under the month labels.
                    topPadding: 18
                    spacing: 3

                    Repeater {
                        model: heatMap.days.slice(0, 7)

                        Text {
                            required property var modelData
                            required property int index

                            height: 15
                            verticalAlignment: Text.AlignVCenter
                            // Every other row, so the labels stay apart.
                            text: index % 2 === 0
                                  ? Date.fromLocaleDateString(Qt.locale("en_GB"), modelData.date, "yyyy-MM-dd")
                                        .toLocaleDateString(Qt.locale("en_GB"), "ddd")
                                  : ""
                            font.family: Theme.type.ui
                            font.pixelSize: Theme.type.small - 1
                            color: Theme.color.inkMuted
                        }
                    }
                }

                Column {
                    spacing: 3

                    Row {
                        objectName: "monthLabels"
                        height: 15
                        spacing: 3

                        Repeater {
                            // The first day of each column.
                            model: heatMap.days.filter((day, at) => at % 7 === 0)

                            Item {
                                id: column

                                required property var modelData
                                required property int index
                                // The month starts in this column. The first column has a label
                                // too, unless a month starts soon after, where the two would touch.
                                readonly property bool starts: heatMap.days.slice(index * 7, index * 7 + 7)
                                    .some(day => day.date.endsWith("-01"))
                                    || (index === 0 && !heatMap.days.slice(7, 21).some(day => day.date.endsWith("-01")))

                                width: 15
                                height: 15

                                Text {
                                    visible: column.starts
                                    text: Date.fromLocaleDateString(Qt.locale("en_GB"),
                                                                    heatMap.days[Math.min(heatMap.days.length - 1, column.index * 7 + 6)].date,
                                                                    "yyyy-MM-dd").toLocaleDateString(Qt.locale("en_GB"), "MMM")
                                    font.family: Theme.type.ui
                                    font.pixelSize: Theme.type.small - 1
                                    color: Theme.color.inkMuted
                                }
                            }
                        }
                    }

                    // The net words of each of the last 26 weeks, oldest first, a
                    // column for each week. A darker square is a day with more words.
                    Grid {
                        id: heatMap

                        readonly property var days: dialog.opened ? dialog.session.historyDays(182) : []
                        // The most words of a day, 0 with no history.
                        readonly property int best: Math.max(0, ...days.map(day => day.words))

                        objectName: "heatMap"
                        rows: 7
                        flow: Grid.TopToBottom
                        spacing: 3

                        Repeater {
                            model: heatMap.days

                            Rectangle {
                                required property var modelData

                                width: 15
                                height: 15
                                radius: 3
                                // When the dialog closes, best falls to 0 before the cells go.
                                color: modelData.words > 0
                                       ? Qt.alpha(Theme.color.accent,
                                                  0.25 + 0.75 * Math.min(1, modelData.words / Math.max(1, heatMap.best)))
                                       : Theme.color.surface
                                border.width: modelData.words > 0 ? 0 : 1
                                border.color: Theme.color.rule
                                Accessible.role: Accessible.StaticText
                                Accessible.name: modelData.date + ": " + modelData.words + " words"
                            }
                        }
                    }
                }
            }

            Note {
                objectName: "streak"
                text: (dialog.session.streak === 1 ? "1 day in a row" : dialog.session.streak + " days in a row")
                      + " with words."
                      + (heatMap.best > 0 ? "  Best day: " + heatMap.best.toLocaleString(Qt.locale("en_GB"), "f", 0) + " words." : "")
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
