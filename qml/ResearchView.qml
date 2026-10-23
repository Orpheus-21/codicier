pragma ComponentBehavior: Bound

import QtQuick

// A file of the research section, inside the app: an image fit to the page,
// or the pages of a PDF in a column. Another file, or a PDF in a build with
// no PDF viewer, opens in the app of the system.
Item {
    id: research

    Accessible.role: Accessible.Pane
    Accessible.name: "Research file"

    required property ProjectSession session
    required property string itemId

    readonly property url file: itemId !== "" ? session.fileUrl(itemId) : ""
    readonly property string suffix: {
        const path = file.toString().toLowerCase();
        return path.slice(path.lastIndexOf(".") + 1);
    }
    readonly property bool image: ["png", "jpg", "jpeg", "webp", "gif", "bmp", "svg"].includes(suffix)
    readonly property bool pdf: suffix === "pdf" && Pdf.available
    readonly property int pages: pdf ? Pdf.pageCount(file) : 0

    Rectangle {
        anchors.fill: parent
        color: Theme.color.paper
    }

    Image {
        objectName: "researchImage"
        visible: research.image
        anchors.fill: parent
        anchors.margins: Theme.space.xl
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        source: research.image ? research.file : ""
    }

    // The pages of a PDF. Each page is drawn at the width of the column when
    // it comes into view.
    ListView {
        id: pageList

        objectName: "pdfPages"
        visible: research.pages > 0
        anchors.fill: parent
        anchors.topMargin: Theme.space.l
        clip: true
        spacing: Theme.space.l
        boundsBehavior: Flickable.StopAtBounds
        activeFocusOnTab: true
        model: research.pages
        Keys.onPressed: event => {
            if (event.key === Qt.Key_PageDown || event.key === Qt.Key_Space)
                contentY = Math.min(contentY + height * 0.9, Math.max(0, contentHeight - height));
            else if (event.key === Qt.Key_PageUp)
                contentY = Math.max(0, contentY - height * 0.9);
            else
                return;
            event.accepted = true;
        }

        // The list places each delegate at x 0, so the page centres itself
        // in a row as wide as the list.
        delegate: Item {
            id: sheet

            required property int index
            readonly property size points: Pdf.pageSize(research.file, index)

            objectName: "pdfPage-" + index
            width: pageList.width
            height: paper.height

            Rectangle {
                id: paper

                width: Math.min(sheet.width - 2 * Theme.space.xl, 760)
                height: sheet.points.width > 0 ? width * sheet.points.height / sheet.points.width : width * 1.414
                x: (sheet.width - width) / 2
                color: "white"
                border.width: 1
                border.color: Theme.color.rule
                Accessible.role: Accessible.Graphic
                Accessible.name: "Page " + (sheet.index + 1) + " of " + research.pages

                PdfPage {
                    anchors.fill: parent
                    anchors.margins: 1
                    file: research.file
                    page: sheet.index
                }
            }
        }

        FocusRing {
            visible: pageList.activeFocus
        }
    }

    // Any other file: its name and a way to open it.
    Column {
        visible: !research.image && research.pages === 0
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * Theme.space.xl, 420)
        spacing: Theme.space.l

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: research.itemId !== "" ? (research.session.fields(research.itemId).title ?? "") : ""
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.title
            color: Theme.color.ink
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: research.suffix === "pdf" ? "This build of the app has no PDF viewer, so the file opens in the PDF app of the system."
                : "This kind of file opens in its own app."
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.inkMuted
        }

        Button {
            objectName: "openExternally"
            anchors.horizontalCenter: parent.horizontalCenter
            kind: "primary"
            text: "Open the file"
            onClicked: Qt.openUrlExternally(research.file)
        }
    }
}
