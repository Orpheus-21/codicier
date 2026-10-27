pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Templates as T

// Exports the whole manuscript, in reading order, to one file: Markdown,
// DOCX for a word processor, EPUB for e-readers, or PDF. Scenes left out of
// the manuscript stay out.
T.Dialog {
    id: dialog

    title: "Export"

    required property ProjectSession session
    property string format: "docx"
    property string error: ""
    property string done: ""
    // The book of a series to export, or "" for the whole manuscript.
    property string book: ""
    property var books: []
    // Each format: its key, its name, its file suffix, and what it is for.
    readonly property var formats: [
        ["docx", "Word (DOCX)", "docx", "For Word and LibreOffice, and for anyone who asks for a Word file."],
        ["epub", "EPUB", "epub", "An e-book for e-readers."],
        ["pdf", "PDF", "pdf", "Pages to read or to print, in the font and the size of the page."],
        ["markdown", "Markdown", "md", "Plain text with marks, for other writing tools."]]
    readonly property var chosen: formats.find(each => each[0] === format)

    function send(file: url) {
        const chapters = session.printChapters(dialog.book);
        const info = session.bookInfo();
        const picked = books.find(each => each.id === dialog.book);
        if (picked)
            info.title = picked.title;
        error = format === "pdf"
            ? Printer.print(chapters, Theme.type.reading, Settings.readingSize, info.sceneBreak, "", file)
            : Exporter.write(format, chapters, info, file);
        done = error === "" ? "Saved " + decodeURIComponent(file.toString().replace("file://", "")) : "";
    }

    implicitWidth: 520
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    padding: Theme.space.xl
    modal: true
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    onAboutToShow: {
        error = "";
        done = "";
        books = session.books();
        if (!books.some(each => each.id === book))
            book = "";
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

    FileDialog {
        id: fileDialog

        title: "Export the manuscript"
        fileMode: FileDialog.SaveFile
        defaultSuffix: dialog.chosen[2]
        nameFilters: [dialog.chosen[1] + " (*." + dialog.chosen[2] + ")"]
        onAccepted: dialog.send(selectedFile)
    }

    contentItem: Column {
        spacing: Theme.space.l

        Text {
            text: "Export the manuscript"
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.title
            color: Theme.color.ink
        }

        Row {
            spacing: Theme.space.xs

            Repeater {
                model: dialog.formats

                Choice {
                    required property var modelData

                    objectName: "format-" + modelData[0]
                    label: modelData[1]
                    chosen: dialog.format === modelData[0]
                    onClicked: dialog.format = modelData[0]
                }
            }
        }

        Row {
            visible: dialog.books.length > 0
            spacing: Theme.space.xs

            Choice {
                objectName: "book-all"
                label: "The whole series"
                chosen: dialog.book === ""
                onClicked: dialog.book = ""
            }

            Repeater {
                model: dialog.books

                Choice {
                    required property var modelData

                    objectName: "book-" + modelData.id
                    label: modelData.title
                    chosen: dialog.book === modelData.id
                    onClicked: dialog.book = modelData.id
                }
            }
        }

        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: dialog.chosen[3] + " The file holds every chapter in reading order, without the scenes that are left out."
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.inkMuted
        }

        Text {
            objectName: "exportResult"
            visible: dialog.error !== "" || dialog.done !== ""
            width: parent.width
            wrapMode: Text.Wrap
            text: dialog.error !== "" ? dialog.error : dialog.done
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            color: dialog.error !== "" ? Theme.color.accent : Theme.color.inkMuted
        }

        Row {
            anchors.right: parent.right
            spacing: Theme.space.s

            Button {
                text: dialog.done !== "" ? "Done" : "Cancel"
                onClicked: dialog.close()
            }

            Button {
                objectName: "exportButton"
                kind: "primary"
                text: "Export…"
                onClicked: fileDialog.open()
            }
        }
    }
}
