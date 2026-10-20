pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// The saved versions of the project, newest first, with the time and the
// message of each.
Item {
    id: panel

    required property ProjectSession session

    // Escape in the panel.
    signal closeRequested

    property var entries: []

    function refresh() {
        entries = session.versionsOn ? session.versions("") : [];
    }

    function start() {
        refresh();
        list.forceActiveFocus();
    }

    Keys.onEscapePressed: closeRequested()

    Connections {
        target: panel.session

        function onVersionsChanged() {
            if (panel.visible)
                panel.refresh();
        }
    }

    Text {
        id: heading

        width: parent.width
        leftPadding: Theme.space.s
        text: "VERSIONS"
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.small
        font.weight: Font.DemiBold
        font.letterSpacing: 1.2
        color: Theme.color.inkMuted
    }

    Text {
        visible: panel.entries.length === 0
        anchors.top: heading.bottom
        anchors.topMargin: Theme.space.l
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: panel.session.versionsOn ? "No version is saved yet."
                                       : "Versions are off. Turn them on in the menu of the project."
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    ListView {
        id: list

        objectName: "versionList"
        anchors.top: heading.bottom
        anchors.topMargin: Theme.space.s
        anchors.bottom: parent.bottom
        width: parent.width
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        activeFocusOnTab: true
        model: panel.entries

        WheelHandler {
            onWheel: event => {
                const end = Math.max(0, list.contentHeight - list.height);
                list.contentY = Math.min(end, Math.max(0, list.contentY - event.angleDelta.y / 2));
            }
        }

        delegate: T.ItemDelegate {
            id: row

            required property var modelData
            required property int index

            width: ListView.view.width
            implicitHeight: implicitContentHeight + topPadding + bottomPadding
            padding: Theme.space.s
            Accessible.name: modelData.message + ", " + modelData.label
            highlighted: ListView.isCurrentItem && list.activeFocus

            contentItem: Column {
                spacing: 2

                Text {
                    width: parent.width
                    text: row.modelData.message
                    elide: Text.ElideRight
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.body
                    color: Theme.color.ink
                }

                Text {
                    width: parent.width
                    text: row.modelData.label
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.small
                    color: Theme.color.inkMuted
                }
            }

            background: Rectangle {
                radius: Theme.radius.small
                color: row.highlighted ? Theme.color.selection : "transparent"
            }
        }
    }
}
