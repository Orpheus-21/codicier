import QtQuick

// A plain editor for the speed probe. The width and the wrap match a real
// chapter view, because a line without wrap is much cheaper to lay out.
Rectangle {
    id: root

    required property string fixture

    width: 900
    height: 800
    color: "#fbf8f1"

    // Put the cursor at pos and scroll it into view, so each key changes a
    // visible line.
    function placeCursor(pos: int) {
        editor.cursorPosition = pos;
        flick.contentY = Math.max(0, editor.cursorRectangle.y - height / 2);
    }

    Flickable {
        id: flick

        anchors.fill: parent
        contentWidth: width
        contentHeight: editor.implicitHeight
        clip: true

        TextEdit {
            id: editor

            objectName: "editor"
            x: 120
            width: 660
            text: root.fixture
            textFormat: TextEdit.PlainText
            wrapMode: TextEdit.Wrap
            font.pixelSize: 18
            color: "#2b2620"
            focus: true
        }
    }
}
