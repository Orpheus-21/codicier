pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Sets the fields of an entity type, or makes a new type with its fields.
// The types and their fields are saved in project.toml.
T.Popup {
    id: dialog

    required property ProjectSession session
    property var types: []
    // The kind of the chosen type, or "" for a new type.
    property string kind: ""

    function choose(type: string) {
        kind = type;
        const chosen = types.find(entry => entry.kind === type);
        nameField.text = chosen ? chosen.name : "";
        fieldsField.text = chosen ? chosen.fields.join(", ") : "";
    }

    function save() {
        const chosen = types.find(entry => entry.kind === kind);
        const saved = session.saveEntityType(kind, nameField.text,
                                             fieldsField.text.split(",").map(text => text.trim()).filter(text => text !== ""));
        if (saved === "")
            return;
        types = session.entityTypes();
        choose(saved);
    }

    implicitWidth: 560
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    padding: Theme.space.xl
    modal: true
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    onAboutToShow: {
        types = session.entityTypes();
        choose(types[0].kind);
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

    component Caps: Text {
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.small
        font.weight: Font.DemiBold
        font.letterSpacing: 1.2
        font.capitalization: Font.AllUppercase
        color: Theme.color.inkMuted
    }

    contentItem: Column {
        spacing: Theme.space.l

        Text {
            text: "Types and fields"
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.title
            color: Theme.color.ink
        }

        Flow {
            width: parent.width
            spacing: Theme.space.xs

            Repeater {
                model: dialog.types

                Choice {
                    required property var modelData

                    label: modelData.name
                    chosen: dialog.kind === modelData.kind
                    onClicked: dialog.choose(modelData.kind)
                }
            }

            Choice {
                objectName: "newType"
                label: "New type"
                chosen: dialog.kind === ""
                onClicked: dialog.choose("")
            }
        }

        Column {
            width: parent.width
            spacing: Theme.space.xs

            Caps {
                text: "Name"
            }

            TextField {
                id: nameField

                objectName: "typeName"
                width: parent.width
                Accessible.name: "Name of the type"
                placeholderText: "Ships"
                // The app names its own types.
                readOnly: dialog.types.some(entry => entry.kind === dialog.kind && !entry.custom)
            }
        }

        Column {
            width: parent.width
            spacing: Theme.space.xs

            Caps {
                text: "Fields"
            }

            TextField {
                id: fieldsField

                objectName: "typeFields"
                width: parent.width
                Accessible.name: "Fields of the type"
                placeholderText: "Fields, split by commas"
                Keys.onReturnPressed: dialog.save()
            }
        }

        Text {
            visible: dialog.session.lastError !== ""
            width: parent.width
            wrapMode: Text.Wrap
            text: dialog.session.lastError
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.accent
        }

        Row {
            anchors.right: parent.right
            spacing: Theme.space.s

            Button {
                text: "Close"
                onClicked: dialog.close()
            }

            Button {
                objectName: "saveType"
                kind: "primary"
                text: dialog.kind === "" ? "Add type" : "Save"
                enabled: dialog.kind !== "" || nameField.text.trim() !== ""
                onClicked: dialog.save()
            }
        }
    }
}
