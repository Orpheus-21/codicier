import QtQuick

// A plain editor for the speed probe. The width and the wrap match a real
// chapter view, because a line without wrap is much cheaper to lay out.
Rectangle {
    id: root

    required property string fixture
    // Characters per line. 60 to 75 is the comfortable range for prose.
    readonly property int measure: 66
    readonly property bool fontsReady: roman.status === FontLoader.Ready
                                       && italic.status === FontLoader.Ready

    width: 900
    height: 800
    color: "#fbf8f1"

    // Put the cursor at pos and scroll it into view, so each key changes a
    // visible line.
    function placeCursor(pos: int) {
        editor.cursorPosition = pos;
        flick.contentY = Math.max(0, editor.cursorRectangle.y - height / 2);
    }

    // Scroll down by dy. At the end, go back to the top, so each step moves
    // the view.
    function scrollStep(dy: real) {
        const end = flick.contentHeight - flick.height;
        flick.contentY = flick.contentY + dy > end ? 0 : flick.contentY + dy;
    }

    FontLoader {
        id: roman

        source: "fonts/CrimsonPro.ttf"
    }

    FontLoader {
        id: italic

        source: "fonts/CrimsonPro-Italic.ttf"
    }

    // The average width of a lowercase letter or space sets the line width.
    FontMetrics {
        id: metrics

        font: editor.font
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
            // The measure in pixels, but never wider than the window less a margin.
            width: Math.min(Math.ceil(metrics.advanceWidth("abcdefghijklmnopqrstuvwxyz ") / 27
                                      * root.measure), flick.width - 48)
            x: Math.max(0, (flick.width - width) / 2)
            text: root.fixture
            textFormat: TextEdit.PlainText
            wrapMode: TextEdit.Wrap
            font.family: roman.font.family
            font.pixelSize: 21
            color: "#2b2620"
            focus: true
        }
    }
}
