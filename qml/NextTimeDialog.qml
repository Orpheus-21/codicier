pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Leaves one line for the next visit. The project shows it when it opens.
T.Dialog {
    id: dialog

    title: "Note for next time"

    // The line that the project holds now.
    property string note: ""

    // The author kept the line. An empty line removes the note.
    signal written(text: string)

    function keep() {
        written(field.text.trim());
        close();
    }

    implicitWidth: 480
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    padding: Theme.space.xl
    modal: true
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    onAboutToShow: {
        field.text = note;
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
            text: "Note for next time"
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.title
            color: Theme.color.ink
        }

        TextField {
            id: field

            objectName: "nextTimeField"
            width: parent.width
            Accessible.name: "Note for next time"
            placeholderText: "Where to start, such as \"Fix the ending of chapter 3\""
            Keys.onReturnPressed: dialog.keep()
            Keys.onEnterPressed: dialog.keep()
        }

        Row {
            anchors.right: parent.right
            spacing: Theme.space.s

            Button {
                text: "Cancel"
                onClicked: dialog.close()
            }

            Button {
                objectName: "keepNextTime"
                kind: "primary"
                text: "Keep"
                onClicked: dialog.keep()
            }
        }
    }
}
