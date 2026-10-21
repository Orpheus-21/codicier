pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// The texts under an item as a table. A click on a header sorts the table
// by that column, and a second click reverses the order. A click on a row
// opens the text.
Item {
    id: outliner

    required property ProjectSession session
    required property string itemId

    // A click on a row asks to open the text.
    signal opened(id: string)

    property var rows: []
    // The custom fields that the rows use, after the fixed columns.
    property var fieldNames: []
    readonly property var columns: [
        { key: "title", name: "Title", width: 3 }, { key: "status", name: "Status", width: 1 },
        { key: "pov", name: "POV", width: 1.4 }, { key: "place", name: "Place", width: 1.4 },
        { key: "time", name: "Time", width: 1.2 }, { key: "words", name: "Words", width: 0.9 }
    ].concat(fieldNames.map(name => ({ key: "field:" + name, name: name, width: 1.4 })))
    readonly property real unit: (width - 2 * Theme.space.xl)
                                 / columns.reduce((sum, column) => sum + column.width, 0)
    property string sortKey: ""
    property bool ascending: true

    function value(row: var, key: string): var {
        return key.startsWith("field:") ? (row.fields[key.slice(6)] ?? "") : row[key];
    }

    function refresh() {
        let list = itemId === "" ? [] : session.outlineOf(itemId);
        let names = [];
        for (const row of list) {
            for (const name of Object.keys(row.fields)) {
                if (!names.includes(name))
                    names.push(name);
            }
        }
        fieldNames = names;
        if (sortKey !== "") {
            const sign = ascending ? 1 : -1;
            list = list.slice().sort((a, b) => {
                const x = value(a, sortKey);
                const y = value(b, sortKey);
                return sign * (typeof x === "number" ? x - y : String(x).localeCompare(String(y)));
            });
        }
        rows = list;
    }

    function sortBy(key: string) {
        ascending = sortKey === key ? !ascending : true;
        sortKey = key;
        refresh();
    }

    onItemIdChanged: refresh()
    Component.onCompleted: refresh()

    Connections {
        target: outliner.session

        function onEdited() {
            outliner.refresh();
        }

        // The live count of the open text arrives after the typing stops.
        function onWordsChanged() {
            if (outliner.visible)
                outliner.refresh();
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.color.paper
    }

    Row {
        id: header

        x: Theme.space.xl
        y: Theme.space.l

        Repeater {
            model: outliner.columns

            T.Button {
                id: headerCell

                required property var modelData

                objectName: "header-" + modelData.key
                width: modelData.width * outliner.unit
                height: 32
                Accessible.name: "Sort by " + modelData.name
                onClicked: outliner.sortBy(modelData.key)

                contentItem: Text {
                    leftPadding: Theme.space.s
                    text: headerCell.modelData.name
                          + (outliner.sortKey === headerCell.modelData.key ? (outliner.ascending ? "  ↑" : "  ↓") : "")
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.small
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.2
                    font.capitalization: Font.AllUppercase
                    color: headerCell.hovered ? Theme.color.ink : Theme.color.inkMuted
                }

                background: Rectangle {
                    color: "transparent"

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: Theme.color.rule
                    }

                    FocusRing {
                        anchors.margins: 0
                        visible: headerCell.visualFocus
                    }
                }
            }
        }
    }

    Text {
        visible: outliner.rows.length === 0
        anchors.centerIn: parent
        text: "No text is under this item."
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    ListView {
        id: list

        objectName: "outlinerRows"
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        x: Theme.space.xl
        width: parent.width - 2 * Theme.space.xl
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: outliner.rows

        WheelHandler {
            onWheel: event => {
                const end = Math.max(0, list.contentHeight - list.height);
                list.contentY = Math.min(end, Math.max(0, list.contentY - event.angleDelta.y / 2));
            }
        }

        delegate: T.ItemDelegate {
            id: row

            required property var modelData

            width: ListView.view.width
            height: 34
            Accessible.name: modelData.title
            highlighted: modelData.id === outliner.session.currentId
            onClicked: outliner.opened(modelData.id)

            contentItem: Row {
                Repeater {
                    model: outliner.columns

                    Text {
                        required property var modelData

                        width: modelData.width * outliner.unit
                        height: row.height
                        leftPadding: Theme.space.s
                        rightPadding: Theme.space.s
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        text: {
                            const value = outliner.value(row.modelData, modelData.key);
                            return modelData.key === "words"
                                ? Number(value).toLocaleString(Qt.locale("en_GB"), "f", 0) : String(value);
                        }
                        font.family: modelData.key === "title" ? Theme.type.reading : Theme.type.ui
                        font.pixelSize: modelData.key === "title" ? Theme.type.body + 1 : Theme.type.body
                        font.capitalization: modelData.key === "status" ? Font.Capitalize : Font.MixedCase
                        color: modelData.key === "title" ? Theme.color.ink : Theme.color.inkMuted
                    }
                }
            }

            background: Rectangle {
                color: row.highlighted ? Theme.color.selection
                     : row.hovered ? Theme.color.surface : "transparent"

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.color.rule
                    opacity: 0.6
                }
            }
        }
    }
}
