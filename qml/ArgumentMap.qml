pragma ComponentBehavior: Bound

import QtQuick

// The argument of an essay: the thesis, each claim with the evidence that
// supports it, the counter-arguments, and the conclusion. A claim with no
// evidence is marked, so the gaps of the argument show at a glance.
Item {
    id: map

    Accessible.role: Accessible.Pane
    Accessible.name: "Argument map"

    required property ProjectSession session

    // The author opens an item of the map.
    signal opened(string id)

    property var argument: ({ thesis: [], claims: [], counters: [], conclusions: [], loose: [] })
    readonly property int unsupported: argument.claims.filter(claim => claim.evidence.length === 0).length

    function refresh() {
        argument = session.argumentMap();
    }

    onVisibleChanged: if (visible) refresh()
    Component.onCompleted: refresh()

    Connections {
        target: map.session

        function onEdited() {
            map.refresh();
        }
    }

    // A heading of a part of the argument.
    component Heading: Text {
        topPadding: Theme.space.l
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.small
        font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 0.6
        color: Theme.color.inkMuted
    }

    // An item of the map: a click, or Return with the focus, opens it.
    component Entry: Text {
        id: entry

        required property var modelData
        property bool large: false

        width: parent.width
        wrapMode: Text.Wrap
        text: modelData.title
        font.family: large ? Theme.type.reading : Theme.type.ui
        font.pixelSize: large ? Theme.type.large : Theme.type.body
        font.underline: hover.hovered
        color: Theme.color.ink
        Accessible.role: Accessible.Link
        Accessible.name: "Open " + modelData.title

        HoverHandler {
            id: hover

            cursorShape: Qt.PointingHandCursor
        }

        TapHandler {
            onTapped: map.opened(entry.modelData.id)
        }

        Activator {
            onActivated: map.opened(entry.modelData.id)
        }
    }

    // A note for an empty part.
    component Empty: Text {
        width: parent.width
        wrapMode: Text.Wrap
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.small
        color: Theme.color.inkMuted
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.color.paper
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: Theme.space.xl
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentHeight: column.implicitHeight

        Column {
            id: column

            width: Math.min(parent.width, 720)
            x: (parent.width - width) / 2
            spacing: Theme.space.s

            Text {
                objectName: "argumentSummary"
                width: parent.width
                wrapMode: Text.Wrap
                text: (map.argument.claims.length === 1 ? "1 claim" : map.argument.claims.length + " claims")
                      + (map.unsupported > 0 ? ", " + map.unsupported + " with no evidence" : "")
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.body
                color: map.unsupported > 0 ? Theme.color.accent : Theme.color.inkMuted
            }

            Heading {
                text: "Thesis"
            }

            Repeater {
                model: map.argument.thesis

                Entry {
                    large: true
                }
            }

            Empty {
                visible: map.argument.thesis.length === 0
                text: "No thesis yet. Mark a text as the thesis in the inspector."
            }

            Heading {
                text: "Claims and evidence"
            }

            Empty {
                visible: map.argument.claims.length === 0
                text: "No claims yet. Mark a text as a claim in the inspector."
            }

            Repeater {
                model: map.argument.claims

                Column {
                    id: claim

                    required property var modelData

                    objectName: "claim-" + modelData.id
                    width: column.width
                    bottomPadding: Theme.space.s
                    spacing: Theme.space.xs

                    Entry {
                        modelData: claim.modelData
                        large: true
                    }

                    Text {
                        objectName: "noEvidence"
                        visible: claim.modelData.evidence.length === 0
                        leftPadding: Theme.space.l
                        text: "No evidence yet"
                        font.family: Theme.type.ui
                        font.pixelSize: Theme.type.small
                        font.weight: Font.DemiBold
                        color: Theme.color.accent
                    }

                    Column {
                        width: parent.width
                        leftPadding: Theme.space.l
                        spacing: Theme.space.xs

                        Repeater {
                            model: claim.modelData.evidence

                            Entry {
                                width: claim.width - Theme.space.l
                                color: Theme.color.inkMuted
                            }
                        }
                    }
                }
            }

            Heading {
                text: "Counter-arguments"
            }

            Repeater {
                model: map.argument.counters

                Entry {}
            }

            Empty {
                visible: map.argument.counters.length === 0
                text: "None yet. A strong essay answers the best case against it."
            }

            Heading {
                text: "Conclusion"
            }

            Repeater {
                model: map.argument.conclusions

                Entry {}
            }

            Empty {
                visible: map.argument.conclusions.length === 0
                text: "No conclusion yet."
            }

            Heading {
                visible: map.argument.loose.length > 0
                text: "Evidence for no claim"
            }

            Repeater {
                model: map.argument.loose

                Entry {
                    color: Theme.color.inkMuted
                }
            }
        }
    }
}
