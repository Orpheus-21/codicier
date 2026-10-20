pragma ComponentBehavior: Bound

import QtQuick

// Figures of the manuscript: the length of each chapter and scene. The view
// reads the files when it opens.
Item {
    id: insights

    required property ProjectSession session

    // A click on a chapter or a scene asks to open it.
    signal opened(id: string)

    property var figures: ({ chapters: [] })
    readonly property int labelWidth: 220
    readonly property int valueWidth: 90
    readonly property int most: Math.max(1, ...figures.chapters.map(chapter => chapter.words))

    function refresh() {
        figures = session.insights();
    }

    function number(n: int): string {
        return Number(n).toLocaleString(Qt.locale("en_GB"), "f", 0);
    }

    onVisibleChanged: if (visible) refresh()
    Component.onCompleted: refresh()

    component Heading: Text {
        topPadding: Theme.space.l
        bottomPadding: Theme.space.s
        font.family: Theme.type.reading
        font.pixelSize: Theme.type.large + 2
        color: Theme.color.ink
    }

    // One labelled bar. The value is text, not color.
    component Bar: Item {
        id: bar

        required property string label
        required property int value
        required property int maximum
        required property string itemId
        property bool child: false

        width: parent.width
        height: child ? 24 : 30
        Accessible.role: Accessible.StaticText
        Accessible.name: label + ": " + insights.number(value) + " words"

        Text {
            x: bar.child ? Theme.space.l : 0
            width: insights.labelWidth - x - Theme.space.s
            anchors.verticalCenter: parent.verticalCenter
            text: bar.label
            elide: Text.ElideRight
            font.family: bar.child ? Theme.type.ui : Theme.type.reading
            font.pixelSize: bar.child ? Theme.type.small + 1 : Theme.type.body + 1
            color: bar.child ? Theme.color.inkMuted : Theme.color.ink
        }

        Rectangle {
            id: fill

            x: insights.labelWidth
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(bar.value > 0 ? 4 : 0,
                            (parent.width - insights.labelWidth - insights.valueWidth) * bar.value / Math.max(1, bar.maximum))
            height: bar.child ? 8 : 14
            radius: 4
            color: Theme.color.accent
            opacity: bar.child ? 0.55 : 1
        }

        Text {
            x: fill.x + fill.width + Theme.space.s
            anchors.verticalCenter: parent.verticalCenter
            text: insights.number(bar.value)
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            color: Theme.color.inkMuted
        }

        TapHandler {
            onTapped: if (bar.itemId !== "") insights.opened(bar.itemId)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.color.paper
    }

    Flickable {
        id: flick

        anchors.fill: parent
        anchors.margins: Theme.space.xl
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentHeight: column.implicitHeight

        WheelHandler {
            onWheel: event => {
                const end = Math.max(0, flick.contentHeight - flick.height);
                flick.contentY = Math.min(end, Math.max(0, flick.contentY - event.angleDelta.y / 2));
            }
        }

        Column {
            id: column

            width: Math.min(flick.width, 900)

            Heading {
                topPadding: 0
                text: "Length"
            }

            Text {
                visible: insights.figures.chapters.length === 0
                text: "The manuscript has no scenes yet."
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.body
                color: Theme.color.inkMuted
            }

            Repeater {
                model: insights.figures.chapters

                Column {
                    id: chapter

                    required property var modelData

                    objectName: "lengthChapter"
                    width: parent.width
                    bottomPadding: Theme.space.s

                    Bar {
                        label: chapter.modelData.title
                        value: chapter.modelData.words
                        maximum: insights.most
                        itemId: chapter.modelData.id
                    }

                    Repeater {
                        model: chapter.modelData.scenes

                        Bar {
                            required property var modelData

                            child: true
                            label: modelData.title
                            value: modelData.words
                            maximum: insights.most
                            itemId: modelData.id
                        }
                    }
                }
            }
        }
    }
}
