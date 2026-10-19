import QtQuick
import QtQuick.Templates as T

// One choice of a set, such as "Left out". chosen marks the current one.
T.ItemDelegate {
    id: option

    required property string label
    required property bool chosen

    implicitWidth: optionText.implicitWidth + 2 * Theme.space.s
    implicitHeight: 30

    background: Rectangle {
        radius: Theme.radius.small
        color: option.chosen ? Theme.color.selection
             : option.hovered ? Qt.alpha(Theme.color.selection, 0.5) : "transparent"
    }

    contentItem: Item {
        Text {
            id: optionText

            anchors.centerIn: parent
            text: option.label
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small + 1
            color: Theme.color.ink
        }
    }
}
