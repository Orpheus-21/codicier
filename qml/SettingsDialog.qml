pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Templates as T

// The settings of the app. Each change applies at once and is saved.
T.Popup {
    id: dialog

    // The template has no size of its own: take the content plus padding,
    // but never more than the window.
    implicitWidth: 560
    implicitHeight: Math.min(implicitContentHeight + topPadding + bottomPadding,
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

    // A label and a control under it.
    component Setting: Column {
        id: setting

        required property string label
        default property alias control: holder.data

        spacing: Theme.space.xs

        Caps {
            text: setting.label
        }

        Row {
            id: holder

            spacing: Theme.space.xs
        }
    }

    // A box with a value and an arrow, as a closed list.
    component Select: T.Button {
        id: select

        implicitWidth: 236
        implicitHeight: 34
        leftPadding: Theme.space.s
        rightPadding: Theme.space.s

        contentItem: Item {
            Text {
                anchors.left: parent.left
                anchors.right: arrow.left
                anchors.rightMargin: Theme.space.s
                anchors.verticalCenter: parent.verticalCenter
                text: select.text
                elide: Text.ElideRight
                font: select.font
                color: Theme.color.ink
            }

            Icon {
                id: arrow

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                name: "chevron-down"
                size: 15
                color: Theme.color.inkMuted
            }
        }

        background: Rectangle {
            radius: Theme.radius.small
            color: select.hovered ? Qt.alpha(Theme.color.selection, 0.5) : Theme.color.paper
            border.width: select.visualFocus ? 2 : 1
            border.color: select.visualFocus ? Theme.color.focus : Theme.color.rule
        }
    }

        // One end of a stepper.
    component StepButton: T.Button {
            id: stepButton

            implicitWidth: 34
            implicitHeight: 32
            focusPolicy: Qt.TabFocus

            contentItem: Text {
                text: stepButton.text
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.large
                color: stepButton.hovered ? Theme.color.ink : Theme.color.inkMuted
            }

            background: Rectangle {
                color: stepButton.hovered || stepButton.down ? Theme.color.selection : "transparent"
            }
        }

    // A value in a box, with a smaller and a larger button at the ends.
    component Stepper: Rectangle {
        id: stepper

        required property string text
        required property string name
        signal step(delta: int)

        implicitWidth: 236
        implicitHeight: 34
        radius: Theme.radius.small
        color: Theme.color.paper
        border.width: 1
        border.color: Theme.color.rule
        clip: true

        StepButton {
            x: 1
            y: 1
            text: "−"
            Accessible.name: "Less " + stepper.name
            onClicked: stepper.step(-1)
        }

        Text {
            anchors.centerIn: parent
            text: stepper.text
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.ink
        }

        StepButton {
            x: parent.width - width - 1
            y: 1
            text: "+"
            Accessible.name: "More " + stepper.name
            onClicked: stepper.step(1)
        }
    }

    FolderDialog {
        id: folderDialog

        title: "Choose the backup folder"
        onAccepted: Settings.setBackupFolderUrl(selectedFolder)
    }

    FontPicker {
        id: readingPicker

        parent: T.Overlay.overlay
        defaultFamily: Theme.readingFont.font.family
        family: Settings.readingFont
        onChosen: family => Settings.readingFont = family
    }

    FontPicker {
        id: uiPicker

        parent: T.Overlay.overlay
        defaultFamily: Theme.uiFont.font.family
        family: Settings.uiFont
        onChosen: family => Settings.uiFont = family
    }

    contentItem: Item {
        implicitHeight: flick.contentHeight + Theme.space.l + doneButton.height

        Flickable {
            id: flick

            anchors.top: parent.top
            anchors.bottom: doneButton.top
            anchors.bottomMargin: Theme.space.l
            width: parent.width
            clip: true
            contentHeight: form.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            WheelHandler {
                onWheel: event => {
                    const end = Math.max(0, flick.contentHeight - flick.height);
                    flick.contentY = Math.min(end, Math.max(0, flick.contentY - event.angleDelta.y / 2));
                }
            }

            Column {
                id: form

                width: parent.width
                spacing: Theme.space.l

                Text {
                    text: "Settings"
                    font.family: Theme.type.reading
                    font.pixelSize: Theme.type.title
                    color: Theme.color.ink
                }

                Heading {
                    text: "Appearance"
                }

                Setting {
                    label: "Theme"

                    Repeater {
                        model: [["system", "System"], ["light", "Light"], ["dark", "Dark"], ["sepia", "Sepia"]]

                        Choice {
                            required property var modelData

                            objectName: "theme-" + modelData[0]
                            label: modelData[1]
                            chosen: Settings.theme === modelData[0]
                            onClicked: Settings.theme = modelData[0]
                        }
                    }
                }

                Grid {
                    columns: 2
                    columnSpacing: Theme.space.l
                    rowSpacing: Theme.space.l

                    Setting {
                        label: "Font of the text"

                        Select {
                            objectName: "readingFontButton"
                            text: Theme.type.reading
                            font.family: Theme.type.reading
                            font.pixelSize: Theme.type.body + 1
                            onClicked: readingPicker.open()
                        }
                    }

                    Setting {
                        label: "Font of the menus"

                        Select {
                            text: Theme.type.ui
                            font.family: Theme.type.ui
                            font.pixelSize: Theme.type.body
                            onClicked: uiPicker.open()
                        }
                    }

                    Setting {
                        label: "Size of the text"

                        Stepper {
                            objectName: "sizeStepper"
                            name: "size"
                            text: Settings.readingSize + " px"
                            onStep: delta => Settings.readingSize += delta
                        }
                    }

                    Setting {
                        label: "Width of the page"

                        Stepper {
                            objectName: "measureStepper"
                            name: "width"
                            text: Settings.measure + " characters"
                            onStep: delta => Settings.measure += 2 * delta
                        }
                    }
                }

                Setting {
                    label: "Animations"

                    Choice {
                        objectName: "motionOn"
                        label: "On"
                        chosen: !Settings.reducedMotion
                        onClicked: Settings.reducedMotion = false
                    }

                    Choice {
                        objectName: "motionOff"
                        label: "Off"
                        chosen: Settings.reducedMotion
                        onClicked: Settings.reducedMotion = true
                    }
                }

                Heading {
                    text: "Writing"
                }

                Setting {
                    label: "Save the text after a pause of"

                    Repeater {
                        model: [[500, "0.5 s"], [1000, "1 s"], [2000, "2 s"], [5000, "5 s"]]

                        Choice {
                            required property var modelData

                            objectName: "autosave-" + modelData[0]
                            label: modelData[1]
                            chosen: Settings.autosaveDelay === modelData[0]
                            onClicked: Settings.autosaveDelay = modelData[0]
                        }
                    }
                }

                Heading {
                    text: "Backups"
                }

                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: "When a project with changes closes, the app writes a zip of it to the backup folder."
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.body
                    color: Theme.color.inkMuted
                }

                Setting {
                    label: "Backup folder"

                    TextField {
                        width: form.width - chooseButton.width - Theme.space.xs
                        readOnly: true
                        text: Settings.backupFolder
                    }

                    Button {
                        id: chooseButton

                        text: "Choose"
                        onClicked: folderDialog.open()
                    }
                }

                Setting {
                    label: "Backups to keep for each project"

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
            }
        }

        Button {
            id: doneButton

            anchors.right: parent.right
            anchors.bottom: parent.bottom
            kind: "primary"
            text: "Done"
            onClicked: dialog.close()
        }
    }
}
