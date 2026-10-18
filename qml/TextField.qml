import QtQuick
import QtQuick.Templates as T

// A one-line text field. iconName puts an icon before the text.
T.TextField {
    id: control

    property string iconName: ""

    implicitWidth: 240
    implicitHeight: 34
    leftPadding: iconName !== "" ? Theme.space.s + 15 + Theme.space.s : Theme.space.s
    rightPadding: Theme.space.s
    verticalAlignment: TextInput.AlignVCenter
    font.family: Theme.type.ui
    font.pixelSize: Theme.type.body
    color: Theme.color.ink
    placeholderTextColor: Theme.color.inkMuted
    selectionColor: Theme.color.selection
    selectedTextColor: Theme.color.ink

    background: Rectangle {
        radius: Theme.radius.small
        color: Theme.color.paper
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.color.focus : Theme.color.rule

        // The template draws no placeholder, so draw it here.
        Text {
            x: control.leftPadding
            width: parent.width - control.leftPadding - control.rightPadding
            height: parent.height
            visible: control.length === 0 && control.preeditText === ""
            text: control.placeholderText
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            font: control.font
            color: control.placeholderTextColor
        }

        Icon {
            visible: control.iconName !== ""
            x: Theme.space.s
            anchors.verticalCenter: parent.verticalCenter
            name: control.iconName
            size: 15
            color: Theme.color.inkMuted
        }
    }
}
