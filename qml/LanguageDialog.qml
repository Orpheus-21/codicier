pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Chooses the spelling language of the project from the dictionaries on
// this computer.
T.Popup {
    id: dialog

    required property ProjectSession session
    property var names: []

    // "British English (United Kingdom)" for en_GB, or the code when Qt
    // does not know it.
    function readable(code: string): string {
        const locale = Qt.locale(code);
        return locale.name === code && locale.nativeLanguageName !== ""
            ? locale.nativeLanguageName + " (" + locale.nativeTerritoryName + ")" : code;
    }

    onAboutToShow: names = session.dictionaries()

    implicitWidth: 420
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

    contentItem: Column {
        id: content

        spacing: Theme.space.m

        Text {
            text: "Spelling language"
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.title
            color: Theme.color.ink
        }

        Text {
            visible: dialog.names.length === 0
            width: parent.width
            wrapMode: Text.Wrap
            text: "No dictionary is on this computer. Install a Hunspell dictionary, such as hunspell-en_gb, then open this again."
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.inkMuted
        }

        Repeater {
            model: dialog.names

            ListItem {
                required property string modelData

                objectName: "language-" + modelData
                width: parent.width
                height: 36
                text: dialog.readable(modelData)
                highlighted: dialog.session.dictionary === modelData
                onClicked: {
                    dialog.session.setLanguage(modelData);
                    dialog.close();
                }
            }
        }
    }
}
