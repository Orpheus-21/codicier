pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Chooses a font family from the fonts on the computer. Each name shows in
// its own font. The first entry is the font that comes with the app.
T.Popup {
    id: picker

    // The name of the font that comes with the app, such as "Literata".
    required property string defaultFamily
    // The chosen family, or "" for the font that comes with the app.
    property string family: ""

    signal chosen(family: string)

    property var shown: []

    function filter() {
        const query = field.text.trim().toLowerCase();
        const all = Qt.fontFamilies().filter(name => name !== defaultFamily);
        shown = [""].concat(all).filter(
            name => (name === "" ? defaultFamily : name).toLowerCase().includes(query));
        list.currentIndex = Math.max(0, shown.indexOf(family));
        list.positionViewAtIndex(list.currentIndex, ListView.Center);
    }

    function choose(row: int) {
        if (row < 0 || row >= shown.length)
            return;
        chosen(shown[row]);
        close();
    }

    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: 420
    height: Math.min(480, parent.height - 2 * Theme.space.l)
    padding: Theme.space.s
    modal: true
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    onAboutToShow: {
        field.text = "";
        filter();
        field.forceActiveFocus();
    }

    T.Overlay.modal: Rectangle {
        color: Qt.alpha(Theme.color.ink, 0.25)
    }

    background: Rectangle {
        radius: Theme.radius.medium
        color: Theme.color.paper
        border.width: 1
        border.color: Theme.color.rule
    }

    contentItem: Item {
        TextField {
            id: field

            objectName: "fontField"
            width: parent.width
            iconName: "search"
            placeholderText: "Find a font"
            onTextChanged: picker.filter()
            Keys.onDownPressed: list.currentIndex = Math.min(list.currentIndex + 1, picker.shown.length - 1)
            Keys.onUpPressed: list.currentIndex = Math.max(list.currentIndex - 1, 0)
            Keys.onReturnPressed: picker.choose(list.currentIndex)
            Keys.onEnterPressed: picker.choose(list.currentIndex)
        }

        ListView {
            id: list

            objectName: "fontList"
            anchors.top: field.bottom
            anchors.topMargin: Theme.space.s
            anchors.bottom: parent.bottom
            width: parent.width
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            highlightMoveDuration: 0
            model: picker.shown

            WheelHandler {
                onWheel: event => {
                    const end = Math.max(0, list.contentHeight - list.height);
                    list.contentY = Math.min(end, Math.max(0, list.contentY - event.angleDelta.y / 2));
                }
            }

            delegate: T.ItemDelegate {
                id: row

                required property string modelData
                required property int index

                width: ListView.view.width
                implicitHeight: 36
                leftPadding: Theme.space.s
                rightPadding: Theme.space.s
                highlighted: ListView.isCurrentItem
                onClicked: picker.choose(index)

                contentItem: Text {
                    text: row.modelData === "" ? picker.defaultFamily + " (comes with the app)" : row.modelData
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    font.family: row.modelData === "" ? picker.defaultFamily : row.modelData
                    font.pixelSize: Theme.type.large
                    font.weight: row.modelData === picker.family ? Font.DemiBold : Font.Normal
                    color: Theme.color.ink
                }

                background: Rectangle {
                    radius: Theme.radius.small
                    color: row.highlighted || row.down ? Theme.color.selection : "transparent"
                }
            }
        }
    }
}
