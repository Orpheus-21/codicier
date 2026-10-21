pragma ComponentBehavior: Bound

import QtQuick

// One scene in the composite view: its title, a conflict bar when needed,
// and its text, which saves to the file of the scene.
Column {
    id: block

    required property int index
    required property var modelData
    required property ProjectSession session
    property bool readOnly: false
    readonly property alias text: pageText

    // The cursor tried to leave the text at the top or at the bottom.
    signal leaveUp
    signal leaveDown

    spacing: Theme.space.m

    // The scene break of the project, between this scene and the one before.
    Text {
        objectName: "sceneBreakMark"
        visible: block.index > 0 && block.session.sceneBreak !== ""
        x: pageText.x
        width: pageText.width
        horizontalAlignment: Text.AlignHCenter
        bottomPadding: Theme.space.l
        text: block.session.sceneBreak === "***" ? "*\u2003*\u2003*" : block.session.sceneBreak
        font.family: Theme.type.reading
        font.pixelSize: Theme.type.body + 2
        color: Theme.color.inkMuted
    }

    Text {
        x: pageText.x
        text: block.modelData.title
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.small
        font.weight: Font.DemiBold
        font.letterSpacing: 1.2
        font.capitalization: Font.AllUppercase
        color: Theme.color.inkMuted
    }

    Row {
        visible: pageText.document.conflict
        x: pageText.x
        spacing: Theme.space.s

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "This text changed in another program."
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            color: Theme.color.ink
        }

        Button {
            text: "Keep mine"
            kind: "primary"
            onClicked: pageText.document.keepMine()
        }

        Button {
            text: "Use the file"
            onClicked: pageText.document.useFile()
        }
    }

    PageText {
        id: pageText

        Accessible.name: "Text of " + (block.modelData.title ?? "the scene")

        availableWidth: block.width - 2 * Theme.space.xl
        x: Math.max(Theme.space.xl, (block.width - width) / 2)
        readOnly: block.readOnly
        document: Document {}
        Component.onCompleted: {
            document.open(block.session.folder, block.modelData.file);
            block.session.trackDocument(document, block.modelData.id);
        }

        Keys.onUpPressed: event => {
            event.accepted = cursorRectangle.y < 2;
            if (event.accepted)
                block.leaveUp();
        }
        Keys.onDownPressed: event => {
            event.accepted = cursorRectangle.y + cursorRectangle.height > height - 2;
            if (event.accepted)
                block.leaveDown();
        }
    }
}
