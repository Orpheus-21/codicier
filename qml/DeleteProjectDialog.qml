pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Deletes a project for good, after the author types its title by hand.
// Paste and every other insert of more than one character do not work, so a
// delete cannot happen by accident.
T.Dialog {
    id: dialog

    title: "Delete the project"

    required property ProjectList projects
    property string folder: ""
    property string projectTitle: ""
    property string error: ""

    function ask(projectFolder: string, name: string) {
        folder = projectFolder;
        projectTitle = name;
        open();
    }

    function remove() {
        if (field.text !== projectTitle)
            return;
        error = projects.remove(folder, Settings.backupFolder);
        if (error === "")
            close();
    }

    implicitWidth: 520
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    padding: Theme.space.xl
    modal: true
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    onAboutToShow: {
        field.text = "";
        field.last = "";
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
            width: parent.width
            wrapMode: Text.Wrap
            text: "Delete " + dialog.projectTitle + "?"
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.title
            color: Theme.color.ink
        }

        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: "This deletes the project folder and every backup of the project. Nothing goes to a trash, and you cannot undo it."
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.inkMuted
        }

        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: "Type the title of the project to confirm: <b>" + dialog.projectTitle.replace(/&/g, "&amp;").replace(/</g, "&lt;") + "</b>"
            textFormat: Text.StyledText
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.ink
        }

        TextField {
            id: field

            // The text before the last edit, to undo an insert of more than
            // one character.
            property string last: ""

            objectName: "confirmTitle"
            width: parent.width
            Accessible.name: "Title of the project to delete"
            onTextEdited: {
                if (text.length > last.length + 1)
                    text = last;
                last = text;
            }
            Keys.onPressed: event => {
                if (event.matches(StandardKey.Paste)
                        || (event.key === Qt.Key_Insert && (event.modifiers & Qt.ShiftModifier)))
                    event.accepted = true;
            }
            Keys.onReturnPressed: dialog.remove()
            Keys.onEnterPressed: dialog.remove()

            // The middle button pastes the selection on Linux: it does nothing here.
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.MiddleButton
            }
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

        Row {
            anchors.right: parent.right
            spacing: Theme.space.s

            Button {
                text: "Cancel"
                onClicked: dialog.close()
            }

            Button {
                objectName: "confirmDelete"
                kind: "primary"
                text: "Delete for good"
                enabled: field.text === dialog.projectTitle
                onClicked: dialog.remove()
            }
        }
    }
}
