pragma ComponentBehavior: Bound

import QtQuick

// The image of a map, fit to the page.
Item {
    id: map

    required property ProjectSession session
    required property string itemId

    Rectangle {
        anchors.fill: parent
        color: Theme.color.paper
    }

    Image {
        id: image

        objectName: "mapImage"
        anchors.fill: parent
        anchors.margins: Theme.space.xl
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        source: map.itemId !== "" ? map.session.fileUrl(map.itemId) : ""
        Accessible.role: Accessible.Graphic
        Accessible.name: "Map"
    }

    Text {
        visible: image.status === Image.Error
        anchors.centerIn: parent
        text: "The image of this map cannot be read."
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.accent
    }
}
