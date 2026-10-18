import QtQuick
import QtQuick.Templates as T

// A square button with one icon. label names the action for screen readers.
T.Button {
    id: control

    required property string iconName
    required property string label

    implicitWidth: Theme.space.xxl
    implicitHeight: Theme.space.xxl
    Accessible.name: label

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: control.iconName
            size: 18
            color: control.hovered || control.down ? Theme.color.ink : Theme.color.inkMuted
        }
    }

    background: Rectangle {
        radius: Theme.radius.small
        color: control.hovered || control.down ? Theme.color.selection : "transparent"
        opacity: control.enabled ? 1 : 0.5

        FocusRing {
            visible: control.visualFocus
        }
    }
}
