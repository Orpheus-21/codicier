import QtQuick

// The keyboard way to a thing that takes a click, such as a card or a node:
// Tab reaches it, Return or Space runs it, and a ring shows the focus. It
// fills its parent and takes the name of the parent, unless one is given.
Item {
    id: activator

    property string name: parent ? parent.Accessible.name : ""
    signal activated

    anchors.fill: parent
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: name
    Accessible.onPressAction: activated()
    Keys.onReturnPressed: activated()
    Keys.onEnterPressed: activated()
    Keys.onSpacePressed: activated()

    FocusRing {
        visible: activator.activeFocus
    }
}
