pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// The draft passes: the list with the color of each, a new pass, and the
// switch for the tints on the page.
T.Dialog {
    id: dialog

    title: "Draft passes"

    required property ProjectSession session
    property var passes: []

    function start() {
        if (session.startPass(field.text) > 0) {
            field.text = "";
            passes = session.passes();
            session.showPasses = true;
        }
    }

    implicitWidth: 480
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    padding: Theme.space.xl
    modal: true
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    onAboutToShow: {
        passes = session.passes();
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
            text: "Draft passes"
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.title
            color: Theme.color.ink
        }

        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: "A pass is one round of revision. When a pass starts, the app keeps a snapshot of each text, so the page can tint the words of each pass in its own color."
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.inkMuted
        }

        // The passes, each with the swatch of its tint.
        Column {
            objectName: "passList"
            width: parent.width
            spacing: Theme.space.xs

            Repeater {
                model: dialog.passes

                Row {
                    id: pass

                    required property var modelData

                    spacing: Theme.space.s

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 14
                        height: 14
                        radius: 3
                        color: Theme.color.series[(pass.modelData.number - 1) % Theme.color.series.length]
                    }

                    Text {
                        text: "Pass " + pass.modelData.number + ": " + pass.modelData.name
                        font.family: Theme.type.ui
                        font.pixelSize: Theme.type.body
                        color: Theme.color.ink
                    }
                }
            }
        }

        Row {
            width: parent.width
            spacing: Theme.space.s

            TextField {
                id: field

                objectName: "passName"
                width: parent.width - startButton.width - parent.spacing
                Accessible.name: "Name of the new pass"
                placeholderText: "Name of the pass, such as \"Structure\""
                Keys.onReturnPressed: dialog.start()
            }

            Button {
                id: startButton

                objectName: "startPass"
                text: "Start the pass"
                enabled: field.text.trim() !== ""
                onClicked: dialog.start()
            }
        }

        Row {
            spacing: Theme.space.xs

            Choice {
                objectName: "showPasses"
                label: "Tint the passes"
                chosen: dialog.session.showPasses
                onClicked: dialog.session.showPasses = true
            }

            Choice {
                label: "No tints"
                chosen: !dialog.session.showPasses
                onClicked: dialog.session.showPasses = false
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
