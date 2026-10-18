import QtQuick

// The keyboard focus mark: a ring just outside its parent.
Rectangle {
    anchors.fill: parent
    anchors.margins: -3
    radius: Theme.radius.small + 3
    color: "transparent"
    border.width: 2
    border.color: Theme.color.focus
}
