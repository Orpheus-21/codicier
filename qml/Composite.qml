pragma ComponentBehavior: Bound

import QtQuick

// All the text items below a chapter or a part, as one scroll. Each scene
// keeps its own file: a title in small capitals, then its text.
ListView {
    id: composite

    required property ProjectSession session
    // The container to show.
    required property string itemId
    // T39 makes the scenes editable.
    property bool readOnly: true

    objectName: "composite"
    model: session.textsUnder(itemId)
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    topMargin: Theme.space.xxxl
    bottomMargin: Theme.space.xxxl
    spacing: Theme.space.xxl
    reuseItems: false

    // A binder edit, such as a new scene in this chapter, reads the list
    // again.
    Connections {
        target: composite.session

        function onEdited() {
            composite.model = composite.session.textsUnder(composite.itemId);
        }
    }

    delegate: Column {
        id: block

        required property var modelData

        width: ListView.view.width
        spacing: Theme.space.m

        Text {
            x: text_.x
            text: block.modelData.title
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            font.weight: Font.DemiBold
            font.letterSpacing: 1.2
            font.capitalization: Font.AllUppercase
            color: Theme.color.inkMuted
        }

        PageText {
            id: text_

            availableWidth: block.width - 2 * Theme.space.xl
            x: Math.max(Theme.space.xl, (block.width - width) / 2)
            readOnly: composite.readOnly
            document: Document {}
            Component.onCompleted: document.open(composite.session.folder, block.modelData.file)
        }
    }
}
