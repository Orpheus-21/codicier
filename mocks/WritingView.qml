pragma ComponentBehavior: Bound

import QtQuick
import Codicier

// Mock: the writing view with the binder, the editor, and the status bar.
// Static data, no actions. For design review only.
Rectangle {
    id: root

    width: 1440
    height: 960
    color: Theme.color.paper

    // Rich text that looks like the styled Markdown of the real editor.
    function mark(s: string): string {
        return `<span style="color:${Theme.color.mark}">${s}</span>`;
    }
    function em(s: string): string {
        return `${mark("*")}<i>${s}</i>${mark("*")}`;
    }
    function strong(s: string): string {
        return `${mark("**")}<b>${s}</b>${mark("**")}`;
    }
    function note(n: int): string {
        return `<sup><span style="color:${Theme.color.accent}">[^${n}]</span></sup>`;
    }

    // Rich text adds its own space after each paragraph, so the line height
    // comes from CSS here and not from Text.lineHeight.
    // In the real editor an empty line splits two paragraphs, so the gap
    // after each paragraph is one line.
    readonly property int lineGap: Math.round(Theme.type.readingSize * Theme.type.readingLineHeight)
    readonly property string manuscript: `<div style="line-height:${Theme.type.readingLineHeight * 100}%">` + [
        `<p style="font-size:30px; margin-bottom:${root.lineGap}px">${mark("## ")}<b>A Date She Did Not Know</b></p>`,
        `<p style="margin-bottom:${root.lineGap}px">The letter said ${em("the fourth of March")}, and nothing else on the first line. Mara read it twice, then held the paper to the window, as if the light could find the year that the ink had left out.</p>`,
        `<p style="margin-bottom:${root.lineGap}px">“She never wrote a date without a reason,” said Tomas. He stood in the door of the river office with his hat in his hands, the way men stood at funerals.${note(1)}</p>`,
        `<p style="margin-bottom:${root.lineGap}px">“Then this is the reason.” She put the letter on the ledger, beside the March accounts. ${strong("The numbers matched.")} Every sum her mother had written in that column added up to the same small figure, month after month, and the figure was not money.</p>`,
        `<p style="margin-bottom:${root.lineGap}px">Outside, the river went on with its long argument against the stones.</p>`
    ].join("") + "</div>"

    component Label: Text {
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    component Caps: Label {
        font.pixelSize: Theme.type.small
        font.weight: Font.DemiBold
        font.letterSpacing: 1.2
        font.capitalization: Font.AllUppercase
    }

    component Rule: Rectangle {
        height: 1
        color: Theme.color.rule
    }

    component IconButton: Item {
        id: button

        required property string iconName

        width: Theme.space.xxl
        height: Theme.space.xxl

        Icon {
            anchors.centerIn: parent
            name: button.iconName
            size: 18
            color: Theme.color.inkMuted
        }
    }

    // One row of the binder. depth sets the indent, words is optional.
    component BinderRow: Rectangle {
        id: binderRow

        required property string label
        property int depth: 0
        property string iconName: ""
        property string words: ""
        property bool open: false
        property bool hasChildren: false
        property bool selected: false

        width: parent.width
        height: 32
        radius: Theme.radius.small
        color: selected ? Theme.color.selection : "transparent"

        Row {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.s + binderRow.depth * Theme.space.l
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space.xs

            Item {
                width: 14
                height: 14
                anchors.verticalCenter: parent.verticalCenter

                Icon {
                    visible: binderRow.hasChildren
                    name: binderRow.open ? "chevron-down" : "chevron-right"
                    size: 14
                    color: Theme.color.inkMuted
                }
            }

            Icon {
                visible: binderRow.iconName !== ""
                anchors.verticalCenter: parent.verticalCenter
                name: binderRow.iconName
                size: 16
                color: Theme.color.inkMuted
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: binderRow.width - binderRow.depth * Theme.space.l - 3 * Theme.space.xl
                       - (binderRow.iconName !== "" ? 20 : 0)
                elide: Text.ElideRight
                text: binderRow.label
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.body
                font.weight: binderRow.selected ? Font.Medium : Font.Normal
                color: Theme.color.ink
            }
        }

        Label {
            anchors.right: parent.right
            anchors.rightMargin: Theme.space.s
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: Theme.type.small
            text: binderRow.words
        }
    }

    // The binder: the structure of the manuscript and the codex.
    Rectangle {
        id: binder

        width: 320
        height: parent.height - statusBar.height
        color: Theme.color.surface

        Column {
            x: Theme.space.m
            y: Theme.space.l
            width: parent.width - 2 * Theme.space.m
            spacing: 2

            Row {
                leftPadding: Theme.space.s
                spacing: Theme.space.s

                Text {
                    text: "The Salt Ledger"
                    font.family: Theme.type.reading
                    font.pixelSize: Theme.type.large + 3
                    color: Theme.color.ink
                }

                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "chevron-down"
                    size: 14
                    color: Theme.color.inkMuted
                }
            }

            Item {
                width: 1
                height: Theme.space.l
            }

            Caps {
                leftPadding: Theme.space.s
                bottomPadding: Theme.space.xs
                text: "Manuscript"
            }

            BinderRow { label: "Part One"; hasChildren: true }
            BinderRow { label: "Part Two"; hasChildren: true; open: true }
            BinderRow { label: "11  The Lock Keeper"; depth: 1; hasChildren: true; words: "5,212" }
            BinderRow { label: "12  The March Accounts"; depth: 1; hasChildren: true; open: true; words: "4,960" }
            BinderRow { label: "The River Office"; depth: 2; iconName: "file-text"; words: "1,630" }
            BinderRow { label: "A Date She Did Not Know"; depth: 2; iconName: "file-text"; words: "1,284"; selected: true }
            BinderRow { label: "Rosemary Again"; depth: 2; iconName: "file-text"; words: "2,046" }
            BinderRow { label: "13  High Water"; depth: 1; hasChildren: true; words: "3,880" }
            BinderRow { label: "Part Three"; hasChildren: true }

            Item {
                width: 1
                height: Theme.space.l
            }

            Caps {
                leftPadding: Theme.space.s
                bottomPadding: Theme.space.xs
                text: "Codex"
            }

            BinderRow { label: "Characters"; iconName: "users"; words: "14" }
            BinderRow { label: "Places"; iconName: "map-pin"; words: "9" }
            BinderRow { label: "Notes"; iconName: "sticky-note"; words: "31" }
        }

        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: Theme.color.rule
        }
    }

    // The editor area: a quiet top line, then the page.
    Item {
        anchors.left: binder.right
        anchors.right: parent.right
        height: parent.height - statusBar.height

        Item {
            id: editorBar

            width: parent.width
            height: 48

            Label {
                anchors.left: parent.left
                anchors.leftMargin: Theme.space.xl
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: Theme.type.small + 1
                text: "Part Two   /   12  The March Accounts   /   A Date She Did Not Know"
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: Theme.space.l
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.space.xs

                IconButton {
                    iconName: "maximize-2"
                }

                IconButton {
                    iconName: "panel-right"
                }

                IconButton {
                    iconName: "ellipsis"
                }
            }
        }

        FontMetrics {
            id: metrics

            font.family: Theme.type.reading
            font.pixelSize: Theme.type.readingSize
        }

        Text {
            id: page

            anchors.top: editorBar.bottom
            anchors.topMargin: Theme.space.xxxl
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.ceil(metrics.advanceWidth("abcdefghijklmnopqrstuvwxyz ") / 27 * Theme.type.measure)
            text: root.manuscript
            textFormat: Text.RichText
            wrapMode: Text.Wrap
            font.family: Theme.type.reading
            font.pixelSize: Theme.type.readingSize
            color: Theme.color.ink
        }
    }

    // The status bar: counts on the left, the session goal on the right.
    Item {
        id: statusBar

        anchors.bottom: parent.bottom
        width: parent.width
        height: 36

        Rule {
            width: parent.width
        }

        Label {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.xl
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: Theme.type.small
            text: "Scene 1,284 words    Chapter 4,960    Book 61,032 of 90,000"
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: Theme.space.xl
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space.m

            Label {
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: Theme.type.small
                text: "Today 412 of 1,000 words"
            }

            Item {
                anchors.verticalCenter: parent.verticalCenter
                width: 80
                height: 2

                Rectangle {
                    anchors.fill: parent
                    color: Theme.color.rule
                }

                Rectangle {
                    width: parent.width * 0.412
                    height: parent.height
                    color: Theme.color.accent
                }
            }
        }
    }
}
