pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Asks for one line of text, such as the note for the next visit or an idea.
T.Dialog {
    id: dialog

    title: dialog.heading

    property string heading: ""
    property string hint: ""
    property string action: "Keep"
    // The line that the field starts with.
    property string note: ""

    // The author kept the line. The line can be empty.
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
            text: dialog.heading
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.title
            color: Theme.color.ink
        }

        TextField {
            id: field

            objectName: "lineField"
            width: parent.width
            Accessible.name: dialog.heading
            placeholderText: dialog.hint
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
                objectName: "keepLine"
                kind: "primary"
                text: dialog.action
                onClicked: dialog.keep()
            }
        }
    }
}
