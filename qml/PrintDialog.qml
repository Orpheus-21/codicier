pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Templates as T

// Prints the open chapter or the whole manuscript, on a printer of the
// system or into a PDF file, with the font and the size of the page.
T.Dialog {
    id: dialog

    title: "Print"

    required property ProjectSession session
    // "chapter" or "manuscript".
    property string scope: "chapter"
    // The name of a printer, or "" to save a PDF file.
    property string printer: ""
    // The author clicked a destination, so the printer search keeps it.
    property bool picked: false
    property var chapters: []
    // True when the open item is in a chapter with text to print.
    property bool inChapter: false
    property string error: ""

    function refresh() {
        chapters = session.printChapters(scope === "chapter" ? session.currentId : "");
    }

    function send(pdf: url) {
        error = Printer.print(chapters, Theme.type.reading, Settings.readingSize, session.sceneBreak, printer, pdf);
        if (error === "")
            close();
    }

    implicitWidth: 520
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    padding: Theme.space.xl
    modal: true
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    onScopeChanged: refresh()
    onAboutToShow: {
        error = "";
        // The printers come a moment later; until then PDF is the choice.
        printer = "";
        picked = false;
        Printer.findPrinters();
        // With no chapter open, the whole manuscript prints.
        inChapter = session.currentId !== "" && session.printChapters(session.currentId).length > 0;
        scope = inChapter ? "chapter" : "manuscript";
        refresh();
    }

    Connections {
        target: Printer

        function onPrintersFound() {
            if (!dialog.picked && !Printer.searching)
                dialog.printer = Printer.defaultPrinter;
        }
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
        id: pdfDialog

        title: "Save the PDF file"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "pdf"
        nameFilters: ["PDF files (*.pdf)"]
        onAccepted: dialog.send(selectedFile)
    }

    contentItem: Column {
        spacing: Theme.space.l

        Text {
            text: "Print"
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.title
            color: Theme.color.ink
        }

        Column {
            spacing: Theme.space.xs

            Text {
                text: "WHAT"
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.small
                font.weight: Font.DemiBold
                font.letterSpacing: 0.6
                color: Theme.color.inkMuted
            }

            Row {
                spacing: Theme.space.xs

                Choice {
                    objectName: "printChapter"
                    label: "This chapter"
                    enabled: dialog.inChapter
                    chosen: dialog.scope === "chapter"
                    onClicked: dialog.scope = "chapter"
                }

                Choice {
                    objectName: "printManuscript"
                    label: "The whole manuscript"
                    chosen: dialog.scope === "manuscript"
                    onClicked: dialog.scope = "manuscript"
                }
            }
        }

        Column {
            width: parent.width
            spacing: Theme.space.xs

            Text {
                text: "WHERE"
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.small
                font.weight: Font.DemiBold
                font.letterSpacing: 0.6
                color: Theme.color.inkMuted
            }

            Flow {
                width: parent.width
                spacing: Theme.space.xs

                Repeater {
                    model: Printer.printers

                    Choice {
                        required property string modelData

                        label: modelData
                        chosen: dialog.printer === modelData
                        onClicked: {
                            dialog.picked = true;
                            dialog.printer = modelData;
                        }
                    }
                }

                Choice {
                    objectName: "printPdf"
                    label: "Save as PDF"
                    chosen: dialog.printer === ""
                    onClicked: {
                        dialog.picked = true;
                        dialog.printer = "";
                    }
                }

                Text {
                    objectName: "findingPrinters"
                    visible: Printer.searching
                    height: 30
                    verticalAlignment: Text.AlignVCenter
                    text: "Looking for printers…"
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.small
                    color: Theme.color.inkMuted
                }
            }
        }

        Text {
            objectName: "printSummary"
            width: parent.width
            wrapMode: Text.Wrap
            text: {
                const count = dialog.chapters.length;
                if (count === 0)
                    return "Nothing to print: no scene of it is in the manuscript.";
                const pages = Printer.pageCount(dialog.chapters, Theme.type.reading, Settings.readingSize,
                                                dialog.session.sceneBreak);
                return (count === 1 ? "1 chapter" : count + " chapters") + ", about "
                    + (pages === 1 ? "1 page" : pages + " pages") + ", in " + Theme.type.reading + " at "
                    + Settings.readingSize + " px. Each chapter starts on a new page.";
            }
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.inkMuted
        }

        Text {
            visible: dialog.error !== ""
            width: parent.width
            wrapMode: Text.Wrap
            text: dialog.error
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            color: Theme.color.accent
        }

        Row {
            anchors.right: parent.right
            spacing: Theme.space.s

            Button {
                text: "Cancel"
                onClicked: dialog.close()
            }

            Button {
                objectName: "printButton"
                kind: "primary"
                text: dialog.printer === "" ? "Save the PDF…" : "Print"
                enabled: dialog.chapters.length > 0
                onClicked: dialog.printer === "" ? pdfDialog.open() : dialog.send("")
            }
        }
    }
}
