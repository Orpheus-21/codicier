import QtQuick
import QtQuick.Templates as T
import Codicier

// The base components in their main states, for design review.
Rectangle {
    id: root

    width: 1100
    height: 560
    color: Theme.color.paper

    component Caps: Text {
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.small
        font.weight: Font.DemiBold
        font.letterSpacing: 1.2
        font.capitalization: Font.AllUppercase
        color: Theme.color.inkMuted
    }

    Column {
        x: Theme.space.xxl
        y: Theme.space.xxl
        spacing: Theme.space.xl

        Caps {
            text: "Buttons"
        }

        Row {
            spacing: Theme.space.l

            Button {
                kind: "primary"
                iconName: "pencil"
                text: "Continue writing"
            }

            Button {
                kind: "primary"
                text: "Disabled"
                enabled: false
            }

            Button {
                iconName: "plus"
                text: "New project"
            }

            Button {
                text: "Cancel"
            }

            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                iconName: "search"
                label: "Search"
            }

            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                iconName: "settings"
                label: "Settings"
            }

            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                iconName: "ellipsis"
                label: "More"
            }
        }

        Caps {
            text: "Text fields"
        }

        Row {
            spacing: Theme.space.l

            TextField {
                iconName: "search"
                placeholderText: "Filter the binder"
            }

            TextField {
                text: "The Salt Ledger"
            }
        }

        Row {
            spacing: Theme.space.xxl

            Column {
                spacing: Theme.space.s

                Caps {
                    text: "List items"
                }

                Pane {
                    width: 320
                    padding: Theme.space.s

                    Column {
                        width: parent.width
                        spacing: 2

                        ListItem {
                            width: parent.width
                            text: "12  The March Accounts"
                            trailing: "4,960"
                        }

                        ListItem {
                            width: parent.width
                            depth: 1
                            iconName: "file-text"
                            text: "The River Office"
                            trailing: "1,630"
                        }

                        ListItem {
                            width: parent.width
                            depth: 1
                            iconName: "file-text"
                            text: "A Date She Did Not Know"
                            trailing: "1,284"
                            highlighted: true
                        }

                        ListItem {
                            width: parent.width
                            depth: 1
                            iconName: "file-text"
                            text: "Rosemary Again"
                            trailing: "2,046"
                        }
                    }
                }
            }

            Column {
                spacing: Theme.space.s

                Caps {
                    text: "Menu"
                }

                Item {
                    width: 240
                    height: 140

                    Menu {
                        id: menu

                        visible: true
                        closePolicy: T.Popup.NoAutoClose

                        T.Action {
                            text: "Rename"
                        }

                        T.Action {
                            text: "Duplicate"
                        }

                        T.Action {
                            text: "Move to trash"
                        }
                    }
                }
            }

            Column {
                spacing: Theme.space.s

                Caps {
                    text: "Splitter"
                }

                Splitter {
                    width: 300
                    height: 140

                    Pane {
                        T.SplitView.preferredWidth: 120

                        Caps {
                            text: "Pane"
                        }
                    }

                    Rectangle {
                        color: Theme.color.paper

                        Text {
                            anchors.centerIn: parent
                            text: "Page"
                            font.family: Theme.type.reading
                            font.pixelSize: Theme.type.large
                            color: Theme.color.ink
                        }
                    }
                }
            }
        }
    }
}
