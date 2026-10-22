pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Makes names for characters from the name lists of Wikidata. A click on a
// name adds it to the codex as a new character.
T.Dialog {
    id: dialog

    title: "Names for characters"

    required property ProjectSession session
    property string culture: "English"
    property string gender: "any"
    property bool invented: false
    property var names: []

    function more() {
        names = generator.generate(culture, gender, 12, invented);
    }

    function addCharacter(name: string) {
        const id = session.add("character");
        if (id !== "")
            session.rename(id, name);
        close();
    }

    onAboutToShow: more()
    onCultureChanged: more()
    onGenderChanged: more()
    onInventedChanged: more()

    NameGenerator {
        id: generator
    }

    implicitWidth: 620
    implicitHeight: content.implicitHeight + topPadding + bottomPadding
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

    contentItem: Column {
        id: content

        spacing: Theme.space.m

        Text {
            text: "Names"
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.title
            color: Theme.color.ink
        }

        Caps {
            text: "Culture"
        }

        Flow {
            width: parent.width
            spacing: Theme.space.xs

            Repeater {
                model: generator.cultures()

                Choice {
                    required property string modelData

                    label: modelData
                    chosen: dialog.culture === modelData
                    onClicked: dialog.culture = modelData
                }
            }
        }

        Flow {
            width: parent.width
            spacing: Theme.space.xs

            Choice {
                label: "Any"
                chosen: dialog.gender === "any"
                onClicked: dialog.gender = "any"
            }

            Choice {
                label: "Female"
                chosen: dialog.gender === "female"
                onClicked: dialog.gender = "female"
            }

            Choice {
                label: "Male"
                chosen: dialog.gender === "male"
                onClicked: dialog.gender = "male"
            }

            Item {
                width: Theme.space.l
                height: 1
            }

            Choice {
                objectName: "realNames"
                label: "Real names"
                chosen: !dialog.invented
                onClicked: dialog.invented = false
            }

            Choice {
                objectName: "inventedNames"
                label: "Invented in this style"
                chosen: dialog.invented
                onClicked: dialog.invented = true
            }
        }

        Caps {
            text: "Click a name to add it to the codex"
        }

        Grid {
            objectName: "nameList"
            width: parent.width
            columns: 3
            columnSpacing: Theme.space.s
            rowSpacing: 2

            Repeater {
                model: dialog.names

                ListItem {
                    required property string modelData

                    width: (parent.width - 2 * Theme.space.s) / 3
                    height: 34
                    text: modelData
                    onClicked: dialog.addCharacter(modelData)
                }
            }
        }

        Row {
            anchors.right: parent.right
            spacing: Theme.space.s

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "Names from Wikidata, public domain"
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.small
                color: Theme.color.inkMuted
            }

            Button {
                objectName: "moreNames"
                iconName: "rotate-ccw-clock"
                text: "More"
                onClicked: dialog.more()
            }

            Button {
                kind: "primary"
                text: "Done"
                onClicked: dialog.close()
            }
        }
    }
}
