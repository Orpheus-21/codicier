pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// The revision checklist of a draft pass: the checks of the pass, and a
// tick box for each check in each chapter.
Item {
    id: checklist

    required property ProjectSession session

    property int pass: 0
    property var passes: []
    property var list: ({ checks: [], chapters: [] })
    readonly property int nameWidth: 220
    readonly property int cellWidth: 150

    function refresh() {
        passes = session.passes();
        if (pass < 1 || pass > passes.length)
            pass = passes.length;
        list = pass > 0 ? session.checklist(pass) : { checks: [], chapters: [] };
    }

    function addCheck(text: string) {
        if (text.trim() === "")
            return;
        session.setPassChecks(pass, list.checks.concat([text.trim()]));
    }

    function removeCheck(text: string) {
        session.setPassChecks(pass, list.checks.filter(check => check !== text));
    }

    onPassChanged: if (pass > 0) list = session.checklist(pass)
    onVisibleChanged: if (visible) refresh()
    Component.onCompleted: refresh()

    Connections {
        target: checklist.session

        function onEdited() {
            checklist.refresh();
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.color.paper
    }

    Text {
        visible: checklist.passes.length === 0
        anchors.centerIn: parent
        text: "Start a draft pass with Ctrl+Alt+D to give it a checklist."
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    Flickable {
        id: flick

        visible: checklist.passes.length > 0
        anchors.fill: parent
        anchors.margins: Theme.space.xl
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: Math.max(width, checklist.nameWidth + checklist.list.checks.length * checklist.cellWidth)
        contentHeight: column.implicitHeight

        Column {
            id: column

            spacing: Theme.space.l

            // The pass to show.
            Flow {
                width: flick.width
                spacing: Theme.space.xs

                Repeater {
                    model: checklist.passes

                    Choice {
                        required property var modelData

                        objectName: "pass-" + modelData.number
                        label: "Pass " + modelData.number + ": " + modelData.name
                        chosen: checklist.pass === modelData.number
                        onClicked: checklist.pass = modelData.number
                    }
                }
            }

            TextField {
                objectName: "newCheck"
                width: Math.min(flick.width, 420)
                Accessible.name: "New check for this pass"
                placeholderText: "New check, such as \"Cut filter words\""
                onAccepted: {
                    checklist.addCheck(text);
                    text = "";
                }
            }

            Grid {
                id: grid

                objectName: "checkGrid"
                columns: checklist.list.checks.length + 1
                rowSpacing: 2
                columnSpacing: 0

                Item {
                    width: checklist.nameWidth
                    height: 36
                }

                Repeater {
                    model: checklist.list.checks

                    Item {
                        id: head

                        required property string modelData

                        width: checklist.cellWidth
                        height: 36

                        Text {
                            anchors.left: parent.left
                            anchors.right: remove.left
                            anchors.verticalCenter: parent.verticalCenter
                            leftPadding: Theme.space.s
                            text: head.modelData
                            elide: Text.ElideRight
                            font.family: Theme.type.ui
                            font.pixelSize: Theme.type.small
                            font.weight: Font.DemiBold
                            color: Theme.color.inkMuted
                        }

                        IconButton {
                            id: remove

                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            iconName: "x"
                            label: "Remove the check " + head.modelData
                            onClicked: checklist.removeCheck(head.modelData)
                        }
                    }
                }

                Repeater {
                    model: checklist.list.chapters.length * (checklist.list.checks.length + 1)

                    Item {
                        id: cell

                        required property int index

                        readonly property int row: Math.floor(index / grid.columns)
                        readonly property int check: index % grid.columns - 1
                        readonly property var chapter: checklist.list.chapters[row]

                        width: check < 0 ? checklist.nameWidth : checklist.cellWidth
                        height: 34

                        Text {
                            visible: cell.check < 0
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - Theme.space.s
                            text: cell.chapter.title
                            elide: Text.ElideRight
                            font.family: Theme.type.reading
                            font.pixelSize: Theme.type.body + 1
                            color: Theme.color.ink
                        }

                        T.AbstractButton {
                            id: box

                            objectName: "tick"
                            visible: cell.check >= 0
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.space.s
                            anchors.verticalCenter: parent.verticalCenter
                            implicitWidth: 22
                            implicitHeight: 22
                            focusPolicy: Qt.StrongFocus
                            checkable: true
                            checked: cell.check >= 0 && cell.chapter.ticked[cell.check]
                            Accessible.role: Accessible.CheckBox
                            Accessible.name: cell.check >= 0
                                ? checklist.list.checks[cell.check] + " in " + cell.chapter.title : ""
                            Accessible.checked: checked
                            onToggled: checklist.session.setTick(checklist.pass, checklist.list.checks[cell.check],
                                                                 cell.chapter.id, checked)

                            background: Rectangle {
                                radius: 4
                                color: box.checked ? Theme.color.accent : Theme.color.paper
                                border.width: 1
                                border.color: box.checked ? Theme.color.accent : Theme.color.rule

                                FocusRing {
                                    anchors.margins: -2
                                    visible: box.visualFocus
                                }
                            }

                            contentItem: Item {
                                Icon {
                                    anchors.centerIn: parent
                                    visible: box.checked
                                    name: "check"
                                    size: 14
                                    color: Theme.color.paper
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
