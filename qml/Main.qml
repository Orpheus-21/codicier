pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Templates as T

Window {
    id: window

    // The last error from opening a folder, shown under the button.
    property string error: ""
    // The folder of the open project, or "" for the workspace home.
    property string projectFolder: ""
    // The title of the open project, for the window title.
    property string projectTitle: ""

    width: 1200
    height: 800
    visible: true
    title: projectTitle !== "" ? projectTitle : "Codicier"
    color: Theme.color.paper

    FolderDialog {
        id: folderDialog

        title: "Choose a workspace folder"
        onAccepted: window.error = Workspaces.openUrl(selectedFolder)
    }

    Loader {
        anchors.fill: parent
        sourceComponent: Workspaces.current === "" ? welcome : window.projectFolder !== "" ? project : home
    }

    // A new workspace closes the open project.
    Connections {
        target: Workspaces

        function onChanged() {
            window.projectFolder = "";
        }
    }

    Component {
        id: project

        ProjectView {
            folder: window.projectFolder
            workspaceName: Workspaces.currentName
            onCloseRequested: window.projectFolder = ""
            Component.onCompleted: window.projectTitle = Qt.binding(() => session.title)
            Component.onDestruction: window.projectTitle = ""
        }
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

            WorkspaceHome {
                id: workspaceHome

                anchors.top: topBar.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                folder: Workspaces.current
                onProjectActivated: folder => window.projectFolder = folder
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: Theme.space.l
                anchors.verticalCenter: topBar.verticalCenter
                spacing: Theme.space.xs

                Button {
                    iconName: "plus"
                    text: "New project"
                    onClicked: workspaceHome.openNewProject()
                }
            }

            Text {
                visible: window.error !== ""
                anchors.centerIn: topBar
                text: window.error
                font.family: Theme.type.ui
                font.pixelSize: Theme.type.body
                color: Theme.color.accent
            }
        }
    }
}
