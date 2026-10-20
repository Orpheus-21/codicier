pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Saves a version of the project with a short message.
T.Popup {
    id: dialog

    required property ProjectSession session
    property string error: ""

    // A version is saved: the message for the status bar.
    signal saved(message: string)

    function save() {
        error = session.saveVersion(field.text);
        if (error === "") {
            saved("Version saved.");
            close();
        }
    }

    implicitWidth: 480
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    padding: Theme.space.xl
    modal: true
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    onAboutToShow: {
        field.text = "";
        error = session.versionsOn ? "" : "Turn on versions first, in the menu of the project.";
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
            text: "Save a version"
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.title
            color: Theme.color.ink
        }

        TextField {
            id: field

            objectName: "versionMessage"
            width: parent.width
            Accessible.name: "What changed"
            placeholderText: "What changed, such as \"New ending\""
            Keys.onReturnPressed: dialog.save()
            Keys.onEnterPressed: dialog.save()
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
                objectName: "saveVersion"
                kind: "primary"
                text: "Save"
                enabled: dialog.session.versionsOn
                onClicked: dialog.save()
            }
        }
    }
}
