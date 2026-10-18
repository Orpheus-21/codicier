pragma ComponentBehavior: Bound

import QtQuick

// The page: the text of the open document in the reading font, in a column
// of the measure width. The document saves itself one second after an edit.
Item {
    id: editorArea

    required property Document document

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

    FontMetrics {
        id: metrics

        font.family: Theme.type.reading
        font.pixelSize: Theme.type.readingSize
    }

    Flickable {
        id: page

        anchors.top: conflictBar.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        contentWidth: width
        contentHeight: textEdit.implicitHeight + 2 * Theme.space.xxxl
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        // Keeps the cursor line in view while the author types.
        function follow(rect: rect) {
            const top = rect.y + Theme.space.xxxl;
            if (top < contentY)
                contentY = top;
            else if (top + rect.height > contentY + height)
                contentY = top + rect.height - height;
        }

        TextEdit {
            id: textEdit

            objectName: "editor"
            y: Theme.space.xxxl
            width: Math.min(Math.ceil(metrics.advanceWidth("abcdefghijklmnopqrstuvwxyz ") / 27
                                      * Theme.type.measure), page.width - 2 * Theme.space.xl)
            x: Math.max(Theme.space.xl, (page.width - width) / 2)
            visible: editorArea.document.file !== ""
            textFormat: TextEdit.PlainText
            wrapMode: TextEdit.Wrap
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.readingSize
            color: Theme.color.ink
            selectionColor: Theme.color.selection
            selectedTextColor: Theme.color.ink
            selectByMouse: true
            persistentSelection: true
            Component.onCompleted: editorArea.document.textDocument = textEdit.textDocument
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
}
