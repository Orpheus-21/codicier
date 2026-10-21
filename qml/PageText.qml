import QtQuick

// The text of one file on the page: the reading font, the theme colours,
// the reading line height, and a column of the measure width. The editor
// and the composite view both use it, so the text looks the same in each.
TextEdit {
    id: pageText

    // The document that loads and saves the text.
    required property Document document
    // The width the text can take. The column is the measure, or less.
    property real availableWidth: 600

    width: Math.min(Math.ceil(metrics.advanceWidth("abcdefghijklmnopqrstuvwxyz ") / 27
                              * Theme.type.measure), availableWidth)
    textFormat: TextEdit.PlainText
    wrapMode: TextEdit.Wrap
    font.family: Theme.type.reading
    font.pixelSize: Theme.type.readingSize
    color: Theme.color.ink
    selectionColor: Theme.color.selection
    selectedTextColor: Theme.color.ink
    selectByMouse: true
    persistentSelection: true
    Component.onCompleted: pageText.document.textDocument = pageText.textDocument
    // The paragraph with the cursor shows its Markdown marks.
    onCursorPositionChanged: pageText.document.cursorAt(activeFocus ? cursorPosition : -1)
    onActiveFocusChanged: pageText.document.cursorAt(activeFocus ? cursorPosition : -1)
    // Save while the text still exists: the text goes before the document.
    Component.onDestruction: pageText.document.save()

    FontMetrics {
        id: metrics

        font.family: Theme.type.reading
        font.pixelSize: Theme.type.readingSize
    }

    // The theme styles the text, and a theme change applies at once.
    Binding {
        target: pageText.document
        property: "markColor"
        value: Theme.color.mark
    }

    Binding {
        target: pageText.document
        property: "accentColor"
        value: Theme.color.accent
    }

    Binding {
        target: pageText.document
        property: "passColors"
        value: Theme.color.series
    }

    // The reading line height, as space added to the natural line.
    Binding {
        target: pageText.document
        property: "lineDistance"
        value: Math.max(0, Theme.type.readingLine - metrics.lineSpacing)
    }
}
