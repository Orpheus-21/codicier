pragma ComponentBehavior: Bound

import QtQuick

// The page: the text of the open document in the reading font, in a column
// of the measure width. The document saves itself one second after an edit.
Item {
    id: editorArea

    required property Document document
    // The place of the cursor in the text, for a split.
    readonly property int cursorPosition: textEdit.cursorPosition
    // Typewriter scroll: the line with the cursor stays at one height.
    property bool typewriter: false
    // That height, as a part of the page height.
    readonly property real typewriterLine: 0.45

    // A conflict: another program changed the file while the text was not
    // saved. The author chooses, and autosave waits.
    Rectangle {
        id: conflictBar

        visible: editorArea.document.conflict
        width: parent.width
        height: visible ? 52 : 0
        color: Theme.color.surface

        Row {
            anchors.centerIn: parent
            spacing: Theme.space.l

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "This text changed in another program."
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.body
                color: Theme.color.ink
            }

            Button {
                text: "Keep mine"
                kind: "primary"
                onClicked: editorArea.document.keepMine()
            }

            Button {
                text: "Use the file"
                onClicked: editorArea.document.useFile()
            }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.color.rule
        }
    }

    Flickable {
        id: page

        objectName: "page"

        anchors.top: conflictBar.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        contentWidth: width
        contentHeight: textEdit.implicitHeight + 2 * Theme.space.xxxl
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        // Keeps the cursor line in view while the author types.
        // In typewriter mode, space above and below the text lets the first and
        // the last lines reach the typewriter line.
        topMargin: editorArea.typewriter ? height * editorArea.typewriterLine : 0
        bottomMargin: editorArea.typewriter ? height * (1 - editorArea.typewriterLine) : 0

        Behavior on contentY {
            enabled: editorArea.typewriter
            NumberAnimation {
                duration: Theme.motion.fast
                easing.type: Theme.motion.easing
            }
        }

        function follow(rect: rect) {
            const top = rect.y + Theme.space.xxxl;
            if (editorArea.typewriter)
                contentY = top - height * editorArea.typewriterLine;
            else if (top < contentY)
                contentY = top;
            else if (top + rect.height > contentY + height)
                contentY = top + rect.height - height;
        }

        PageText {
            id: textEdit

            objectName: "editor"
            Accessible.name: "Text of the page"
            Accessible.multiLine: true
            document: editorArea.document
            availableWidth: page.width - 2 * Theme.space.xl
            y: Theme.space.xxxl
            x: Math.max(Theme.space.xl, (page.width - width) / 2)
            visible: editorArea.document.file !== ""
            onCursorRectangleChanged: page.follow(cursorRectangle)
        }
    }

    Text {
        visible: editorArea.document.file === ""
        anchors.centerIn: parent
        text: "Choose a scene in the binder."
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    Text {
        visible: editorArea.document.error !== ""
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.space.l
        anchors.horizontalCenter: parent.horizontalCenter
        text: editorArea.document.error
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.accent
    }

    // Puts the keyboard focus on the page.
    function focusText() {
        textEdit.forceActiveFocus();
    }

    // Selects length characters from start and puts the focus on the page.
    // A start of -1 only puts the focus on the page.
    function selectRange(start: int, length: int) {
        textEdit.forceActiveFocus();
        if (start < 0)
            return;
        textEdit.select(start, start + length);
        placeCursor();
    }

    // Brings the cursor line to its place, as after a change of mode.
    function placeCursor() {
        page.follow(textEdit.cursorRectangle);
    }

    onTypewriterChanged: Qt.callLater(placeCursor)
}
