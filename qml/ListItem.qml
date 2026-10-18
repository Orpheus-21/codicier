import QtQuick
import QtQuick.Templates as T

// One row of a list or tree, as in the binder. depth sets the indent.
// highlighted marks the current row. trailing is a short text on the right.
T.ItemDelegate {
    id: control

    property string iconName: ""
    property string trailing: ""
    property int depth: 0

    implicitWidth: 280
    implicitHeight: 32
    leftPadding: Theme.space.s + depth * Theme.space.l
    rightPadding: Theme.space.s
    spacing: Theme.space.xs
    font.family: Theme.type.ui
    font.pixelSize: Theme.type.body
    font.weight: highlighted ? Font.Medium : Font.Normal

    contentItem: Item {
        Row {
            anchors.left: parent.left
            anchors.right: trailingText.left
            anchors.rightMargin: Theme.space.s
            anchors.verticalCenter: parent.verticalCenter
            spacing: control.spacing

            Icon {
                visible: control.iconName !== ""
                anchors.verticalCenter: parent.verticalCenter
                name: control.iconName
                size: 16
                color: Theme.color.inkMuted
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - (control.iconName !== "" ? 16 + control.spacing : 0)
                text: control.text
                elide: Text.ElideRight
                font: control.font
                color: Theme.color.ink
            }
        }

        Text {
            id: trailingText

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: control.trailing
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            color: Theme.color.inkMuted
        }
    }

    background: Rectangle {
        radius: Theme.radius.small
        color: control.highlighted || control.down ? Theme.color.selection : "transparent"
        opacity: !control.highlighted && control.hovered ? 0.6 : 1

        Rectangle {
            anchors.fill: parent
            visible: control.hovered && !control.highlighted && !control.down
            radius: parent.radius
            color: Theme.color.selection
        }

        FocusRing {
            anchors.margins: 0
            visible: control.visualFocus
        }
    }
}
