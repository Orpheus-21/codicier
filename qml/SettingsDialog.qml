pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Templates as T

// The settings of the app: for now, the backups.
T.Popup {
    id: dialog

    // The template has no size of its own: take the content plus padding.
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

    FolderDialog {
        id: folderDialog

        title: "Choose the backup folder"
        onAccepted: Settings.setBackupFolderUrl(selectedFolder)
    }

    contentItem: Column {
        spacing: Theme.space.l

        Text {
            text: "Settings"
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.title
            color: Theme.color.ink
        }

        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: "When a project with changes closes, the app writes a zip of it to the backup folder."
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.inkMuted
        }

        Column {
            width: parent.width
            spacing: Theme.space.xs

            Text {
                text: "BACKUP FOLDER"
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.small
                font.weight: Font.DemiBold
                font.letterSpacing: 1.2
                color: Theme.color.inkMuted
            }

            Row {
                width: parent.width
                spacing: Theme.space.s

                TextField {
                    width: parent.width - chooseButton.width - parent.spacing
                    readOnly: true
                    text: Settings.backupFolder
                }

                Button {
                    id: chooseButton

                    text: "Choose"
                    onClicked: folderDialog.open()
                }
            }
        }

        Column {
            width: parent.width
            spacing: Theme.space.xs

            Text {
                text: "BACKUPS TO KEEP FOR EACH PROJECT"
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.small
                font.weight: Font.DemiBold
                font.letterSpacing: 1.2
                color: Theme.color.inkMuted
            }

            TextField {
                width: 120
                text: String(Settings.backupsToKeep)
                validator: IntValidator {
                    bottom: 1
                    top: 1000
                }
                onEditingFinished: Settings.backupsToKeep = parseInt(text)
            }
        }

        Button {
            anchors.right: parent.right
            kind: "primary"
            text: "Done"
            onClicked: dialog.close()
        }
    }
}
