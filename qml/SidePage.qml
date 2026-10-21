pragma ComponentBehavior: Bound

import QtQuick

// A second page next to the main page: one more text to read or edit, with
// its own document, so each page saves on its own.
Item {
    id: side

    required property ProjectSession session
    // The item on this page, or "".
    required property string itemId

    signal closeRequested

    readonly property var fields: itemId !== "" ? session.fields(itemId) : ({})

    onItemIdChanged: {
        // The fields binding may not have the new item yet, so ask the session.
        pageText.document.open(session.folder, itemId !== "" ? (session.fields(itemId).file ?? "") : "");
        if (itemId !== "")
            session.trackDocument(pageText.document, itemId);
    }

    Rectangle {
        width: 1
        height: parent.height
        color: Theme.color.rule
    }

    Item {
        id: header

        width: parent.width
        height: 48

        Text {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.l
            anchors.right: closeButton.left
            anchors.verticalCenter: parent.verticalCenter
            text: side.fields.title ?? ""
            elide: Text.ElideRight
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            font.weight: Font.DemiBold
            font.letterSpacing: 1.2
            font.capitalization: Font.AllUppercase
            color: Theme.color.inkMuted
        }

        IconButton {
            id: closeButton

            objectName: "closeSidePage"
            anchors.right: parent.right
            anchors.rightMargin: Theme.space.l
            anchors.verticalCenter: parent.verticalCenter
            iconName: "x"
            label: "Close the second page"
            onClicked: side.closeRequested()
        }
    }

    Flickable {
        id: flick

        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        contentHeight: pageText.height + 2 * Theme.space.xl
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        WheelHandler {
            onWheel: event => {
                const end = Math.max(0, flick.contentHeight - flick.height);
                flick.contentY = Math.min(end, Math.max(0, flick.contentY - event.angleDelta.y / 2));
            }
        }

        PageText {
            id: pageText

            objectName: "sideText"
            Accessible.name: "Text of the second page"
            availableWidth: flick.width - 2 * Theme.space.xl
            x: Math.max(Theme.space.xl, (flick.width - width) / 2)
            y: Theme.space.l
            document: Document {}
        }
    }
}
