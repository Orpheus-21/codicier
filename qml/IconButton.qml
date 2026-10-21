import QtQuick
import QtQuick.Templates as T

// A button with one icon. label names the action for screen readers and the
// tooltip. keys, when set, shows the key of the action in the tooltip.
// showLabel puts the label next to the icon.
T.Button {
    id: control

    required property string iconName
    required property string label
    property string keys: ""
    property bool showLabel: false

    implicitWidth: showLabel ? 2 * Theme.space.s + 18 + Theme.space.xs + labelText.implicitWidth : Theme.space.xxl
    implicitHeight: Theme.space.xxl
    // The tooltip needs hover, also where the platform turns hover off.
    hoverEnabled: true
    Accessible.name: label

    contentItem: Item {
        Icon {
            id: icon

            x: control.showLabel ? Theme.space.s : (parent.width - width) / 2
            anchors.verticalCenter: parent.verticalCenter
            name: control.iconName
            size: 18
            color: control.hovered || control.down ? Theme.color.ink : Theme.color.inkMuted
        }

        Text {
            id: labelText

            visible: control.showLabel
            x: icon.x + icon.width + Theme.space.xs
            anchors.verticalCenter: parent.verticalCenter
            text: control.label
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            color: icon.color
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

    T.ToolTip {
        id: tip

        objectName: "tooltip"
        visible: control.hovered
        delay: 600
        margins: Theme.space.s
        y: control.height + Theme.space.xs
        x: (control.width - width) / 2
        padding: Theme.space.s
        text: control.label + (control.keys !== "" ? "   " + control.keys : "")

        contentItem: Text {
            text: tip.text
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            color: Theme.color.ink
        }

        background: Rectangle {
            radius: Theme.radius.small
            color: Theme.color.surface
            border.width: 1
            border.color: Theme.color.rule
        }
    }
}
