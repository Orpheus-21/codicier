pragma ComponentBehavior: Bound

import QtQml
import QtQuick
import QtQuick.Templates as T

// The image of a map, fit to the page, with the pins of its places. A right
// click on the map adds a pin there. A click on a pin opens its place, a
// drag moves it, and a right click removes it.
Item {
    id: map

    Accessible.role: Accessible.Pane
    Accessible.name: "Map"

    required property ProjectSession session
    required property string itemId

    // A click on a pin asks to open its place.
    signal opened(id: string)

    property var pins: []
    // Where a right click asked for a new pin, as parts of the image.
    property point newPin: Qt.point(0, 0)
    property int menuPin: -1

    // The rectangle of the image on the page, for the places of the pins.
    readonly property rect area: Qt.rect(image.x + (image.width - image.paintedWidth) / 2,
                                         image.y + (image.height - image.paintedHeight) / 2,
                                         image.paintedWidth, image.paintedHeight)

    function refresh() {
        pins = itemId !== "" ? session.pins(itemId) : [];
    }

    onItemIdChanged: refresh()
    Component.onCompleted: refresh()

    Connections {
        target: map.session

        function onEdited() {
            map.refresh();
        }
    }

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

    // A right click on the image asks which entity to pin there.
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: eventPoint => {
            const p = eventPoint.position;
            if (p.x < map.area.x || p.y < map.area.y || p.x > map.area.x + map.area.width
                    || p.y > map.area.y + map.area.height)
                return;
            map.newPin = Qt.point((p.x - map.area.x) / map.area.width, (p.y - map.area.y) / map.area.height);
            placeMenu.popup(map, p.x, p.y);
        }
    }

    Menu {
        id: placeMenu

        objectName: "placeMenu"

        Instantiator {
            model: map.visible ? map.session.items().filter(item => item.kind === "place") : []

            delegate: T.Action {
                required property var modelData

                text: "Pin " + modelData.title
                onTriggered: map.session.addPin(map.itemId, modelData.id, map.newPin.x, map.newPin.y)
            }

            onObjectAdded: (index, action) => placeMenu.insertAction(index, action as T.Action)
            onObjectRemoved: (index, action) => placeMenu.removeAction(action as T.Action)
        }
    }

    Menu {
        id: pinMenu

        T.Action {
            text: "Remove the pin"
            onTriggered: map.session.removePin(map.menuPin)
        }
    }

    Repeater {
        model: map.pins

        Item {
            id: pin

            required property var modelData

            objectName: "mapPin"
            x: map.area.x + modelData.x * map.area.width - width / 2
            y: map.area.y + modelData.y * map.area.height - 12
            width: Math.max(24, label.implicitWidth + 2 * Theme.space.s)
            height: 44
            Accessible.role: Accessible.Button
            Accessible.name: modelData.title

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 12
                height: 12
                radius: 6
                color: Theme.color.accent
                border.width: 2
                border.color: Theme.color.paper
                y: 6
            }

            Text {
                id: label

                anchors.horizontalCenter: parent.horizontalCenter
                y: 20
                text: pin.modelData.title
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.small
                font.weight: Font.DemiBold
                color: Theme.color.ink
                style: Text.Outline
                styleColor: Theme.color.paper
            }

            TapHandler {
                onTapped: map.opened(pin.modelData.place)
            }

            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: {
                    map.menuPin = pin.modelData.index;
                    pinMenu.popup(pin, 0, pin.height);
                }
            }

            DragHandler {
                id: drag

                onActiveChanged: {
                    if (active)
                        return;
                    const x = (pin.x + pin.width / 2 - map.area.x) / map.area.width;
                    const y = (pin.y + 12 - map.area.y) / map.area.height;
                    map.session.movePin(pin.modelData.index, Math.min(1, Math.max(0, x)), Math.min(1, Math.max(0, y)));
                }
            }
        }
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
