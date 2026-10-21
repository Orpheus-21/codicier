import QtQuick
import QtQuick.Templates as T

// A pop-up menu. Give it Action or MenuItem children.
T.Menu {
    id: control

    implicitWidth: 220
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    padding: Theme.space.xs

    delegate: T.MenuItem {
        id: item

        Accessible.name: text
        implicitWidth: 212
        implicitHeight: 32
        leftPadding: Theme.space.m
        rightPadding: Theme.space.m
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body

        contentItem: Item {
            Text {
                anchors.fill: parent
                anchors.rightMargin: item.checked ? 22 : 0
                text: item.text
                font: item.font
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                color: item.enabled ? Theme.color.ink : Theme.color.inkMuted
            }

            // The chosen item of a group of choices.
            Icon {
                visible: item.checked
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                name: "check"
                size: 16
                color: Theme.color.accent
            }
        }

        background: Rectangle {
            radius: Theme.radius.small
            color: item.highlighted || item.down ? Theme.color.selection : "transparent"
        }
    }

    contentItem: ListView {
        implicitHeight: contentHeight
        model: control.contentModel
        interactive: false
        clip: true
    }

    background: Rectangle {
        radius: Theme.radius.medium
        color: Theme.color.paper
        border.width: 1
        border.color: Theme.color.rule
    }
}
