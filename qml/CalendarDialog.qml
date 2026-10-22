pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Sets the calendar of story time: one month on each line, with its name
// and its number of days. No months is the Gregorian calendar.
T.Dialog {
    id: dialog

    title: "Calendar of the story"

    required property ProjectSession session
    property string error: ""

    // The lines of the field as months, or null for a line that is not valid.
    function parse(text: string): var {
        let months = [];
        for (const line of text.split("\n")) {
            const clean = line.trim();
            if (clean === "")
                continue;
            const match = clean.match(/^(.*\S)\s+(\d+)$/);
            if (!match)
                return null;
            months.push({ name: match[1], days: parseInt(match[2]) });
        }
        return months;
    }

    function save(months: var) {
        if (months === null || !session.setCalendar(months)) {
            error = "Write each month on its own line as a name and the days, such as \"Frostmonth 40\". A month has 1 to 1000 days.";
            return;
        }
        close();
    }

    implicitWidth: 520
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    padding: Theme.space.xl
    modal: true
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    onAboutToShow: {
        field.text = session.calendarMonths().map(month => month.name + " " + month.days).join("\n");
        error = "";
        field.forceActiveFocus();
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
            text: "Calendar of the story"
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.title
            color: Theme.color.ink
        }

        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: "Write one month on each line: the name, then the number of days. A date is then year-month-day, such as 312-1-15 for the 15th day of the first month. With no months, the story uses the Gregorian calendar."
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.inkMuted
        }

        TextArea {
            id: field

            objectName: "calendarMonths"
            width: parent.width
            Accessible.name: "Months of the calendar"
            placeholderText: "Frostmonth 40\nThaw 30"
        }

        Text {
            visible: dialog.error !== ""
            width: parent.width
            wrapMode: Text.Wrap
            text: dialog.error
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.accent
        }

        Item {
            width: parent.width
            height: saveButton.height

            Button {
                objectName: "gregorian"
                text: "Use the Gregorian calendar"
                onClicked: dialog.save([])
            }

            Row {
                anchors.right: parent.right
                spacing: Theme.space.s

                Button {
                    text: "Cancel"
                    onClicked: dialog.close()
                }

                Button {
                    id: saveButton

                    objectName: "saveCalendar"
                    kind: "primary"
                    text: "Save"
                    onClicked: dialog.save(dialog.parse(field.text))
                }
            }
        }
    }
}
