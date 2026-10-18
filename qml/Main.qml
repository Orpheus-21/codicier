pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Templates as T

Window {
    id: window

    // The last error from opening a folder, shown under the button.
    property string error: ""

    width: 1200
    height: 800
    visible: true
    title: "Codicier"
    color: Theme.color.paper

    FolderDialog {
        id: folderDialog

        title: "Choose a workspace folder"
        onAccepted: window.error = Workspaces.openUrl(selectedFolder)
    }

    Loader {
        anchors.fill: parent
        sourceComponent: Workspaces.current === "" ? welcome : home
    }

    // The first start: no workspace yet.
    Component {
        id: welcome

        Item {
            Column {
                anchors.centerIn: parent
                width: 420
                spacing: Theme.space.l

                Text {
                    text: "Codicier"
                    font.family: Theme.type.reading
                    font.pixelSize: Theme.type.display + 12
                    color: Theme.color.ink
                }

                Text {
                    width: parent.width
                    text: "Choose a folder for your writing. Each project in it is a folder of plain text files."
                    wrapMode: Text.Wrap
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.body
                    lineHeight: 1.3
                    color: Theme.color.inkMuted
                }

                Button {
                    kind: "primary"
                    iconName: "folder-open"
                    text: "Choose a folder"
                    onClicked: folderDialog.open()
                }

                Text {
                    visible: window.error !== ""
                    text: window.error
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.body
                    color: Theme.color.accent
                }
            }
        }
    }

    // A workspace is open. T27 adds the projects below the top bar.
    Component {
        id: home

        Item {
            Item {
                id: topBar

                width: parent.width
                height: 56

                Button {
                    id: switcher

                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space.m
                    anchors.verticalCenter: parent.verticalCenter
                    text: Workspaces.currentName
                    iconName: "chevron-down"
                    iconAfterText: true
                    onClicked: switchMenu.popup(switcher, 0, switcher.height)
                }

                Menu {
                    id: switchMenu

                    Instantiator {
                        model: Workspaces.recent
                        delegate: T.Action {
                            required property string modelData

                            text: Workspaces.nameOf(modelData)
                            onTriggered: window.error = Workspaces.open(modelData)
                        }
                        onObjectAdded: (index, action) => switchMenu.insertAction(index, action as T.Action)
                        onObjectRemoved: (index, action) => switchMenu.removeAction(action as T.Action)
                    }

                    T.Action {
                        text: "Open another folder"
                        onTriggered: folderDialog.open()
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.color.rule
                }
            }

            Text {
                anchors.top: topBar.bottom
                anchors.topMargin: Theme.space.xxxl
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(implicitWidth, parent.width - 2 * Theme.space.xl)
                elide: Text.ElideMiddle
                text: window.error !== "" ? window.error : Workspaces.current
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.body
                color: window.error !== "" ? Theme.color.accent : Theme.color.inkMuted
            }
        }
    }
}
