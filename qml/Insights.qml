pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes

import "Reading.js" as Reading

// Figures of the manuscript: the length of each chapter and scene. The view
// reads the files when it opens.
Item {
    id: insights

    Accessible.role: Accessible.Pane
    Accessible.name: "Insights"

    required property ProjectSession session

    // A click on a chapter or a scene asks to open it.
    signal opened(id: string)

    property var figures: ({ chapters: [], povNames: [] })
    // The reading ease is a measure for English only.
    readonly property bool english: session.dictionary.startsWith("en")

    // The Flesch bands, as words.
    function easeWords(ease: real): string {
        return ease >= 90 ? "very easy" : ease >= 80 ? "easy" : ease >= 70 ? "fairly easy"
             : ease >= 60 ? "plain" : ease >= 50 ? "fairly hard" : ease >= 30 ? "hard" : "very hard";
    }
    // The phrases that come back in two or more scenes.
    property var phrases: []
    // The POV names with a color: the five with the most words. The rest
    // share the gray of "Other".
    readonly property var named: figures.povNames.slice(0, 5)

    function povColor(name: string): color {
        const index = named.indexOf(name);
        return index >= 0 ? Theme.color.series[index] : Theme.color.inkMuted;
    }

    // The POV shares of a chapter, in the order of the colors, with the
    // names past the fifth joined as "Other".
    function shares(chapter: var): var {
        let list = named.filter(name => (chapter.pov[name] ?? 0) > 0)
            .map(name => ({ name: name, words: chapter.pov[name] }));
        let other = 0;
        for (const name of Object.keys(chapter.pov)) {
            if (!named.includes(name))
                other += chapter.pov[name];
        }
        if (other > 0)
            list.push({ name: "Other", words: other });
        return list;
    }

    function percent(part: real, whole: real): string {
        return Math.round(100 * part / Math.max(1, whole)) + "%";
    }
    readonly property int labelWidth: 220
    readonly property int valueWidth: 90
    readonly property int most: Math.max(1, ...figures.chapters.map(chapter => chapter.words))

    function refresh() {
        figures = session.insights();
        phrases = session.repeatedPhrases();
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

        // A Repeater makes the bar before it sets the parent.
        width: parent ? parent.width : 0
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
            // A chapter also shows its reading time.
            text: insights.number(bar.value) + (bar.child || bar.value === 0 ? "" : "   " + Reading.label(bar.value))
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            color: Theme.color.inkMuted
        }

        TapHandler {
            onTapped: if (bar.itemId !== "") insights.opened(bar.itemId)
        }

        Activator {
            visible: bar.itemId !== ""
            name: "Open " + bar.label
            onActivated: insights.opened(bar.itemId)
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
        contentHeight: sections.implicitHeight

        WheelHandler {
            onWheel: event => {
                const end = Math.max(0, flick.contentHeight - flick.height);
                flick.contentY = Math.min(end, Math.max(0, flick.contentY - event.angleDelta.y / 2));
            }
        }

        Column {
            id: sections

            width: Math.min(flick.width, 900)

            Heading {
                topPadding: 0
                text: "Length"
            }

            Text {
                objectName: "bookReading"
                readonly property int words: insights.figures.chapters.reduce((sum, chapter) => sum + chapter.words, 0)

                visible: words > 0
                bottomPadding: Theme.space.s
                text: "The manuscript: " + insights.number(words) + " words, " + Reading.label(words) + " to read."
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.body
                color: Theme.color.inkMuted
            }

            Text {
                visible: insights.figures.chapters.length === 0
                text: "The manuscript has no scenes yet."
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.body
                color: Theme.color.inkMuted
            }

            Repeater {
                id: lengths

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

            Heading {
                text: "Point of view"
            }

            // The legend: a swatch and a name for each color.
            Flow {
                objectName: "povLegend"
                visible: insights.named.length > 1
                width: parent.width
                spacing: Theme.space.l
                bottomPadding: Theme.space.s

                Repeater {
                    model: insights.named.concat(insights.figures.povNames.length > 5 ? ["Other"] : [])

                    Row {
                        id: key

                        required property string modelData

                        spacing: Theme.space.xs

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 12
                            height: 12
                            radius: 3
                            color: insights.povColor(key.modelData)
                        }

                        Text {
                            text: key.modelData
                            font.family: Theme.type.ui
                            font.pixelSize: Theme.type.small + 1
                            color: Theme.color.ink
                        }
                    }
                }
            }

            Repeater {
                model: insights.figures.chapters

                Column {
                    id: povChapter

                    required property var modelData

                    readonly property var parts: insights.shares(modelData)

                    objectName: "povChapter"
                    width: parent.width
                    bottomPadding: Theme.space.s
                    Accessible.role: Accessible.StaticText
                    Accessible.name: modelData.title + ": " + parts.map(part => part.name + " "
                        + insights.percent(part.words, modelData.words)).join(", ")

                    Item {
                        width: parent.width
                        height: 30

                        Text {
                            width: insights.labelWidth - Theme.space.s
                            anchors.verticalCenter: parent.verticalCenter
                            text: povChapter.modelData.title
                            elide: Text.ElideRight
                            font.family: Theme.type.reading
                            font.pixelSize: Theme.type.body + 1
                            color: Theme.color.ink
                        }

                        // One segment for each POV, with a 2 pixel gap between.
                        Row {
                            id: stack

                            readonly property real room: parent.width - insights.labelWidth - insights.valueWidth
                                                         - 2 * Math.max(0, povChapter.parts.length - 1)

                            x: insights.labelWidth
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2

                            Repeater {
                                model: povChapter.parts

                                Rectangle {
                                    required property var modelData

                                    width: Math.max(2, stack.room * modelData.words / Math.max(1, povChapter.modelData.words))
                                    height: 14
                                    radius: 4
                                    color: insights.povColor(modelData.name)
                                }
                            }
                        }
                    }

                    // The shares as text, so no value lives in color alone.
                    Text {
                        x: insights.labelWidth
                        text: povChapter.modelData.words === 0 ? "No words yet."
                            : povChapter.parts.map(part => part.name + " "
                                + insights.percent(part.words, povChapter.modelData.words)).join("   ")
                        font.family: Theme.type.ui
                        font.pixelSize: Theme.type.small
                        color: Theme.color.inkMuted
                    }
                }
            }

            Heading {
                text: "Most used words"
            }

            Text {
                visible: (insights.figures.words ?? []).length === 0
                text: "The manuscript has no words yet."
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.body
                color: Theme.color.inkMuted
            }

            Repeater {
                model: insights.figures.words ?? []

                Bar {
                    required property var modelData

                    objectName: "frequentWord"
                    child: true
                    label: modelData.word
                    value: modelData.count
                    maximum: insights.figures.words[0].count
                    itemId: ""
                }
            }

            Heading {
                text: "Readability"
            }

            Repeater {
                model: insights.figures.chapters.filter(chapter => chapter.words > 0)

                Row {
                    id: readable

                    required property var modelData

                    objectName: "readability"
                    width: parent.width
                    height: 28

                    Text {
                        width: 220
                        anchors.verticalCenter: parent.verticalCenter
                        text: readable.modelData.title
                        elide: Text.ElideRight
                        font.family: Theme.type.reading
                        font.pixelSize: Theme.type.body + 1
                        color: Theme.color.ink
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: Math.round(readable.modelData.sentenceLength) + " words a sentence"
                              + (insights.english
                                 ? ", reading ease " + Math.round(readable.modelData.readingEase)
                                   + " (" + insights.easeWords(readable.modelData.readingEase) + ")"
                                 : "")
                        font.family: Theme.type.ui
                        font.pixelSize: Theme.type.body
                        color: Theme.color.inkMuted
                    }
                }
            }

            Heading {
                text: "Repeated phrases"
            }

            Text {
                visible: insights.phrases.length === 0
                text: "No phrase of three words or more comes back in another scene."
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.body
                color: Theme.color.inkMuted
            }

            Repeater {
                model: insights.phrases

                Column {
                    id: phrase

                    required property var modelData

                    objectName: "repeatedPhrase"
                    width: parent.width
                    bottomPadding: Theme.space.s

                    Text {
                        width: parent.width
                        text: "\u201C" + phrase.modelData.phrase + "\u201D, " + phrase.modelData.count + " times"
                        wrapMode: Text.Wrap
                        font.family: Theme.type.reading
                        font.pixelSize: Theme.type.body + 1
                        color: Theme.color.ink
                    }

                    Flow {
                        width: parent.width
                        spacing: Theme.space.m

                        Repeater {
                            model: phrase.modelData.places

                            Text {
                                id: place

                                required property var modelData

                                text: modelData.title
                                font.family: Theme.type.ui
                                font.pixelSize: Theme.type.small
                                font.underline: placeHover.hovered
                                color: Theme.color.accent
                                Accessible.role: Accessible.Link
                                Accessible.name: "Open " + modelData.title
                                Accessible.onPressAction: insights.opened(place.modelData.id)

                                HoverHandler {
                                    id: placeHover

                                    cursorShape: Qt.PointingHandCursor
                                }

                                TapHandler {
                                    onTapped: insights.opened(place.modelData.id)
                                }

                                Activator {
                                    onActivated: insights.opened(place.modelData.id)
                                }
                            }
                        }
                    }
                }
            }

            Heading {
                visible: (insights.figures.names ?? []).length > 0
                text: "Names of the codex"
            }

            Repeater {
                model: insights.figures.names ?? []

                Bar {
                    required property var modelData

                    objectName: "frequentName"
                    child: true
                    label: modelData.word
                    value: modelData.count
                    maximum: insights.figures.names[0].count
                    itemId: modelData.id
                }
            }

            Heading {
                visible: gapRows.count > 0
                text: "Scenes with missing information"
            }

            Repeater {
                id: gapRows

                model: insights.figures.gaps ?? []

                ListItem {
                    required property var modelData

                    objectName: "gapScene"
                    width: parent.width
                    height: 34
                    text: modelData.chapter + ": " + modelData.title + " has no " + modelData.missing.join(", no ")
                    onClicked: insights.opened(modelData.id)
                }
            }

            Heading {
                visible: unfinishedRows.count > 0
                text: "Drafts in finished chapters"
            }

            Repeater {
                id: unfinishedRows

                model: insights.figures.unfinished ?? []

                ListItem {
                    required property var modelData

                    objectName: "unfinishedScene"
                    width: parent.width
                    height: 34
                    text: modelData.chapter + ": " + modelData.title + " is still a draft"
                    onClicked: insights.opened(modelData.id)
                }
            }

            Heading {
                text: "Tension"
            }

            Text {
                visible: (insights.figures.tension ?? []).length === 0
                width: parent.width
                wrapMode: Text.Wrap
                text: "Rate scenes from 1 to 5 in the inspector to see the tension across the book."
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.body
                color: Theme.color.inkMuted
            }

            // One line across the book: the scenes with a rating, in order.
            Item {
                id: tensionChart

                readonly property var points: insights.figures.tension ?? []
                readonly property real inset: 10

                objectName: "tensionChart"
                visible: points.length > 0
                width: parent.width
                height: 140
                Accessible.role: Accessible.StaticText
                Accessible.name: "Tension line of " + points.length + " rated scenes: "
                                 + points.map(point => point.title + " " + point.value).join(", ")

                function xOf(point: var): real {
                    return inset + point.position * (width - 2 * inset);
                }

                function yOf(point: var): real {
                    return height - inset - (point.value - 1) / 4 * (height - 2 * inset);
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.color.rule
                }

                Shape {
                    anchors.fill: parent
                    preferredRendererType: Shape.CurveRenderer

                    ShapePath {
                        strokeColor: Theme.color.accent
                        strokeWidth: 2
                        fillColor: "transparent"

                        PathPolyline {
                            path: tensionChart.points.map(point => Qt.point(tensionChart.xOf(point), tensionChart.yOf(point)))
                        }
                    }
                }

                Repeater {
                    model: tensionChart.points

                    Rectangle {
                        required property var modelData

                        x: tensionChart.xOf(modelData) - 4
                        y: tensionChart.yOf(modelData) - 4
                        width: 8
                        height: 8
                        radius: 4
                        color: Theme.color.accent
                    }
                }
            }

            Heading {
                text: "Dialogue"
            }

            Repeater {
                model: insights.figures.chapters

                Item {
                    id: talk

                    required property var modelData

                    objectName: "dialogueChapter"
                    width: parent.width
                    height: 30
                    Accessible.role: Accessible.StaticText
                    Accessible.name: modelData.title + ": " + insights.percent(modelData.dialogue, 1) + " dialogue"

                    Text {
                        width: insights.labelWidth - Theme.space.s
                        anchors.verticalCenter: parent.verticalCenter
                        text: talk.modelData.title
                        elide: Text.ElideRight
                        font.family: Theme.type.reading
                        font.pixelSize: Theme.type.body + 1
                        color: Theme.color.ink
                    }

                    Rectangle {
                        id: talkFill

                        x: insights.labelWidth
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(talk.modelData.dialogue > 0 ? 4 : 0,
                                        (parent.width - insights.labelWidth - insights.valueWidth) * talk.modelData.dialogue)
                        height: 14
                        radius: 4
                        color: Theme.color.accent
                    }

                    Text {
                        x: talkFill.x + talkFill.width + Theme.space.s
                        anchors.verticalCenter: parent.verticalCenter
                        text: insights.percent(talk.modelData.dialogue, 1) + " dialogue"
                        font.family: Theme.type.ui
                        font.pixelSize: Theme.type.small
                        color: Theme.color.inkMuted
                    }
                }
            }

            Heading {
                text: "Characters in each chapter"
            }

            Text {
                visible: (insights.figures.characters ?? []).length === 0
                text: "Add characters to the codex to see where they appear."
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.body
                color: Theme.color.inkMuted
            }

            // A row for each character and a column for each chapter. A cell
            // shows the count of mentions, on a tint that grows with it.
            Grid {
                id: presence

                readonly property var people: insights.figures.characters ?? []
                readonly property int cellWidth: 72
                readonly property int most: Math.max(1, ...insights.figures.chapters.map(
                    chapter => Math.max(0, ...(chapter.presence ?? []))))

                objectName: "presenceGrid"
                visible: people.length > 0
                columns: insights.figures.chapters.length + 1
                rowSpacing: 2
                columnSpacing: 2

                Item {
                    width: insights.labelWidth
                    height: 28
                }

                Repeater {
                    model: insights.figures.chapters

                    Text {
                        required property var modelData

                        width: presence.cellWidth
                        height: 28
                        verticalAlignment: Text.AlignBottom
                        horizontalAlignment: Text.AlignHCenter
                        text: modelData.title
                        elide: Text.ElideRight
                        font.family: Theme.type.ui
                        font.pixelSize: Theme.type.small
                        font.weight: Font.DemiBold
                        color: Theme.color.inkMuted
                    }
                }

                Repeater {
                    model: presence.people.length * (insights.figures.chapters.length + 1)

                    Rectangle {
                        id: cell

                        required property int index

                        readonly property int row: Math.floor(index / presence.columns)
                        readonly property int column: index % presence.columns - 1
                        readonly property int count: column >= 0
                            ? (insights.figures.chapters[column].presence[row] ?? 0) : 0

                        objectName: column >= 0 ? "presenceCell" : "presenceName"
                        width: column < 0 ? insights.labelWidth : presence.cellWidth
                        height: 28
                        radius: 4
                        color: column < 0 || count === 0 ? "transparent"
                             : Qt.alpha(Theme.color.accent, 0.2 + 0.8 * count / presence.most)
                        Accessible.role: Accessible.StaticText
                        Accessible.name: column < 0 ? presence.people[row].title
                            : presence.people[row].title + " in " + insights.figures.chapters[column].title + ": "
                              + count + (count === 1 ? " mention" : " mentions")

                        Text {
                            anchors.fill: parent
                            horizontalAlignment: cell.column < 0 ? Text.AlignLeft : Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            text: cell.column < 0 ? presence.people[cell.row].title : (cell.count > 0 ? cell.count : "")
                            elide: Text.ElideRight
                            font.family: cell.column < 0 ? Theme.type.reading : Theme.type.ui
                            font.pixelSize: cell.column < 0 ? Theme.type.body + 1 : Theme.type.small
                            color: cell.column >= 0 && cell.count / presence.most > 0.5 ? Theme.color.paper : Theme.color.ink
                        }
                    }
                }
            }
        }
    }
}
