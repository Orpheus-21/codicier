pragma ComponentBehavior: Bound

import QtQuick

// The word difference between a snapshot and the current text, over the
// page. Removed words are struck through in the accent colour, and added
// words sit on the selection colour.
Rectangle {
    id: compare

    required property ProjectSession session
    required property string itemId
    // The snapshot to compare with, or "" to close.
    required property string snapshotPath
    property string snapshotLabel: ""

    signal closeRequested

    readonly property var result: snapshotPath === "" ? ({}) : session.compare(itemId, snapshotPath)

    function escapeHtml(text: string): string {
        return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
                   .replace(/\n/g, "<br>");
    }

    readonly property string html: {
        const segments = result.segments ?? [];
        let out = "";
        for (const segment of segments) {
            const text = escapeHtml(segment.text);
            if (segment.kind === "removed")
                out += `<span style="color:${Theme.color.accent}; text-decoration:line-through">${text}</span>`;
            else if (segment.kind === "added")
                out += `<span style="background-color:${Theme.color.selection}">${text}</span>`;
            else
                out += text;
        }
        return out;
    }

    color: Theme.color.paper

    // The header: what is compared, the counts, and the actions.
    Item {
        id: header

        width: parent.width
        height: 56

        Column {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.xl
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                text: "Changes since " + compare.snapshotLabel
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.body
                font.weight: Font.Medium
                color: Theme.color.ink
            }

            Text {
                text: (compare.result.addedWords ?? 0) + " words added, "
                      + (compare.result.removedWords ?? 0) + " words removed"
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.small
                color: Theme.color.inkMuted
            }
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: Theme.space.l
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space.s

            Button {
                // This button restores a snapshot, not a saved version.
                visible: !compare.snapshotPath.startsWith("version:")
                text: "Restore this version"
                onClicked: {
                    compare.session.restoreSnapshot(compare.itemId, compare.snapshotPath);
                    compare.closeRequested();
                }
            }

            Button {
                kind: "primary"
                text: "Close"
                onClicked: compare.closeRequested()
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
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        contentHeight: body.height + 2 * Theme.space.xxxl
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Text {
            id: body

            objectName: "compareText"
            y: Theme.space.xxxl
            width: Math.min(Math.ceil(metrics.advanceWidth("abcdefghijklmnopqrstuvwxyz ") / 27
                                      * Theme.type.measure), parent.width - 2 * Theme.space.xl)
            x: Math.max(Theme.space.xl, (parent.width - width) / 2)
            text: compare.html
            textFormat: Text.RichText
            lineHeightMode: Text.FixedHeight
            lineHeight: Theme.type.readingLine
            wrapMode: Text.Wrap
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.readingSize
            color: Theme.color.ink
        }
    }

    Keys.onEscapePressed: compare.closeRequested()
}
