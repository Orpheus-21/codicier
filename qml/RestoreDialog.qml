pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Lists the zip backups of a project. A click on a backup unpacks it as a
// new project. The old project stays as it is.
T.Dialog {
    id: dialog

    title: "Restore a backup"

    required property ProjectList projects
    property string folder: ""
    property string projectTitle: ""
    property var backups: []
    property string error: ""

    function ask(projectFolder: string, name: string) {
        folder = projectFolder;
        projectTitle = name;
        error = "";
        backups = projects.backups(projectFolder, Settings.backupFolder);
        open();
    }

    function restore(path: string) {
        error = projects.restoreBackup(path);
        if (error === "")
            close();
    }

    implicitWidth: 520
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
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

    contentItem: Column {
        spacing: Theme.space.l

        Text {
            text: "Restore a backup of " + dialog.projectTitle
            width: parent.width
            wrapMode: Text.Wrap
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.title
            color: Theme.color.ink
        }

        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: dialog.backups.length === 0
                  ? "This project has no backup yet. The app makes one when the project closes."
                  : "Click a backup to unpack it as a new project. The old project does not change."
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.inkMuted
        }

        ListView {
            width: parent.width
            height: Math.min(contentHeight, 280)
            visible: dialog.backups.length > 0
            clip: true
            model: dialog.backups

            delegate: ListItem {
                required property var modelData

                objectName: "backupRow"
                width: ListView.view.width
                text: modelData.label
                onClicked: dialog.restore(modelData.path)
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

        Button {
            anchors.right: parent.right
            text: "Close"
            onClicked: dialog.close()
        }
    }
}
