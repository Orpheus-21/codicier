pragma ComponentBehavior: Bound

import QtQml
import QtQuick

import "Reading.js" as Reading
import QtQuick.Templates as T

// The inspector for the selected item, as in the T18 mock: the synopsis,
// the status, the point of view, the place, the time, the word target, the
// tags, and the notes. A field saves when the author leaves it or presses
// Enter, and the values go to binder.toml.
Pane {
    id: inspector

    required property ProjectSession session

    // The author wants to compare the text with a snapshot.
    signal compareRequested(string path, string label)
    // The fields of the selected item, read again after each change.
    property var fields: ({})

    // The entity type of the selected item, or undefined for another kind.
    property var entityType: undefined

    function refresh() {
        inspector.fields = session.fields(session.currentId);
        inspector.entityType = session.entityTypes().find(type => type.kind === inspector.fields.kind);
        inspector.mentions = inspector.entityType ? session.mentionsOf(session.currentId) : [];
        inspector.plotlines = session.plotlines();
        inspector.relations = inspector.entityType ? session.relations(session.currentId) : [];
    }

    // The relations of the open entity.
    property var relations: []

    // The plotlines of the project, read again after each change.
    property var plotlines: []

    // Puts the open scene on a plotline, or takes it off.
    function togglePlotline(id: string) {
        const now = inspector.fields.plotlines ?? [];
        save("plotlines", now.includes(id) ? now.filter(each => each !== id) : now.concat([id]));
    }

    // The texts that name the open entity, read when the entity opens.
    property var mentions: []

    // Saves one field if its value changed. Returns true if it saved.
    function save(key: string, value: var): bool {
        const id = session.currentId;
        if (id === "" || JSON.stringify(inspector.fields[key]) === JSON.stringify(value))
            return false;
        return session.setField(id, key, value);
    }

    objectName: "inspector"
    padding: Theme.space.xl
    Component.onCompleted: refresh()

    Connections {
        target: inspector.session

        function onCurrentIdChanged() {
            inspector.refresh();
        }

        function onEdited() {
            inspector.refresh();
        }

        function onLoaded() {
            inspector.refresh();
        }
    }

    component Caps: Text {
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.small
        font.weight: Font.DemiBold
        font.letterSpacing: 1.2
        font.capitalization: Font.AllUppercase
        color: Theme.color.inkMuted
    }

    component Rule: Rectangle {
        height: 1
        color: Theme.color.rule
    }

    // A labelled one-line field that saves on Enter or when it loses focus.
    component Field: Column {
        id: field

        required property string label
        required property string key
        property string hint: ""

        spacing: Theme.space.xs

        Caps {
            text: field.label
        }

        TextField {
            width: parent.width
            Accessible.name: field.label
            placeholderText: field.hint
            text: inspector.fields[field.key] ?? ""
            onEditingFinished: inspector.save(field.key, text)
        }
    }

    // The status as the shape from the binder: a pale ring for draft, an
    // accent ring for revised, and an accent dot for final.
    component StatusChoice: T.ItemDelegate {
        id: choice

        Accessible.name: label
        Accessible.role: Accessible.RadioButton
        Accessible.checkable: true
        Accessible.checked: chosen
        focusPolicy: Qt.StrongFocus

        required property string status
        required property string label
        readonly property bool chosen: inspector.fields.status === status

        implicitWidth: row.implicitWidth + 2 * Theme.space.s
        implicitHeight: 30
        onClicked: inspector.save("status", choice.status)

        background: Rectangle {
            radius: Theme.radius.small
            color: choice.chosen ? Theme.color.selection
                 : choice.hovered ? Qt.alpha(Theme.color.selection, 0.5) : "transparent"

            FocusRing {
                anchors.margins: 0
                visible: choice.visualFocus
            }
        }

        contentItem: Item {
            Row {
                id: row

                anchors.centerIn: parent
                spacing: Theme.space.xs

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 8
                    height: 8
                    radius: 4
                    color: choice.status === "final" ? Theme.color.accent : "transparent"
                    border.width: choice.status === "final" ? 0 : 1.5
                    border.color: choice.status === "draft" ? Theme.color.mark : Theme.color.accent
                }

                Text {
                    text: choice.label
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.small + 1
                    color: Theme.color.ink
                }
            }
        }
    }

    Text {
        visible: inspector.session.currentId === ""
        anchors.centerIn: parent
        text: "Choose an item in the binder."
        font.family: Theme.type.ui
        font.pixelSize: Theme.type.body
        color: Theme.color.inkMuted
    }

    Flickable {
        visible: inspector.session.currentId !== ""
        anchors.fill: parent
        contentHeight: column.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: column

            width: parent.width
            spacing: Theme.space.l

            Caps {
                text: inspector.fields.kind ?? ""
            }

            Text {
                width: parent.width
                text: inspector.fields.title ?? ""
                wrapMode: Text.Wrap
                font.family: Theme.type.reading
                font.pixelSize: Theme.type.title
                color: Theme.color.ink
            }

            Column {
                width: parent.width
                spacing: Theme.space.xs

                Caps {
                    text: "Synopsis"
                }

                TextArea {
                    width: parent.width
                    Accessible.name: "Synopsis"
                    text: inspector.fields.synopsis ?? ""
                    placeholderText: inspector.fields.kind === "scene" ? "What happens in this scene" : "A short summary"
                    font.family: Theme.type.reading
                    font.italic: true
                    font.pixelSize: Theme.type.large - 1
                    onEditingFinished: inspector.save("synopsis", text)
                }
            }

            Rule {
                width: parent.width
            }

            Column {
                visible: inspector.fields.kind === "scene"
                width: parent.width
                spacing: Theme.space.xs

                Caps {
                    text: "Status"
                }

                Row {
                    spacing: Theme.space.xs

                    StatusChoice {
                        status: "draft"
                        label: "Draft"
                    }

                    StatusChoice {
                        status: "revised"
                        label: "Revised"
                    }

                    StatusChoice {
                        status: "final"
                        label: "Final"
                    }
                }
            }

            Column {
                visible: inspector.fields.kind === "scene"
                width: parent.width
                spacing: Theme.space.xs

                Caps {
                    text: "Manuscript"
                }

                Row {
                    spacing: Theme.space.xs

                    Choice {
                        label: "In the manuscript"
                        chosen: inspector.fields.include ?? true
                        onClicked: inspector.save("include", true)
                    }

                    Choice {
                        label: "Left out"
                        chosen: !(inspector.fields.include ?? true)
                        onClicked: inspector.save("include", false)
                    }
                }
            }

            Field {
                visible: inspector.fields.kind === "scene"
                width: parent.width
                label: "Point of view"
                key: "pov"
            }

            Field {
                visible: inspector.fields.kind === "scene"
                width: parent.width
                label: "Place"
                key: "place"
            }

            Field {
                visible: inspector.fields.kind === "scene"
                width: parent.width
                label: "Time"
                key: "time"
            }

            // Optional questions of the scene, for the outline.
            Field {
                visible: inspector.fields.kind === "scene"
                width: parent.width
                label: "Goal"
                key: "goal"
                hint: "What the POV character wants here"
            }

            Field {
                visible: inspector.fields.kind === "scene"
                width: parent.width
                label: "Conflict"
                key: "conflict"
                hint: "What stands in the way"
            }

            Field {
                visible: inspector.fields.kind === "scene"
                width: parent.width
                label: "Outcome"
                key: "outcome"
                hint: "Yes, no, yes but, or no and"
            }

            Column {
                // A word target is for the texts of the book, not for an entity.
                visible: inspector.entityType === undefined
                width: parent.width
                spacing: Theme.space.xs

                Item {
                    width: parent.width
                    height: targetLabel.height

                    Caps {
                        id: targetLabel

                        text: "Word target"
                    }

                    Text {
                        objectName: "wordsAndReading"
                        anchors.right: parent.right
                        text: Number(inspector.fields.words ?? 0).toLocaleString(Qt.locale("en_GB"), "f", 0)
                              + ((inspector.fields.target ?? 0) > 0
                                 ? " of " + Number(inspector.fields.target).toLocaleString(Qt.locale("en_GB"), "f", 0)
                                 : " words")
                              + ((inspector.fields.words ?? 0) > 0
                                 ? ", " + Reading.label(inspector.fields.words) + " to read" : "")
                        font.family: Theme.type.ui
                        font.pixelSize: Theme.type.small
                        color: Theme.color.inkMuted
                    }
                }

                Item {
                    visible: (inspector.fields.target ?? 0) > 0
                    width: parent.width
                    height: 2

                    Rectangle {
                        anchors.fill: parent
                        color: Theme.color.rule
                    }

                    Rectangle {
                        width: parent.width * Math.min(1, (inspector.fields.words ?? 0) / Math.max(1, inspector.fields.target ?? 1))
                        height: parent.height
                        color: Theme.color.accent
                    }
                }

                TextField {
                    width: parent.width
                    placeholderText: "No target"
                    Accessible.name: "Word target"
                    text: (inspector.fields.target ?? 0) > 0 ? String(inspector.fields.target) : ""
                    validator: IntValidator {
                        bottom: 0
                        top: 10000000
                    }
                    onEditingFinished: inspector.save("target", text === "" ? 0 : parseInt(text))
                }
            }

            Column {
                visible: inspector.entityType !== undefined
                width: parent.width
                spacing: Theme.space.xs

                Caps {
                    text: "Aliases"
                }

                TextField {
                    objectName: "aliasesField"
                    width: parent.width
                    placeholderText: "Other names, split by commas"
                    Accessible.name: "Aliases"
                    text: (inspector.fields.aliases ?? []).join(", ")
                    onEditingFinished: inspector.save("aliases", text.split(",").map(t => t.trim()).filter(t => t !== ""))
                }
            }

            Column {
                objectName: "dateSection"
                visible: inspector.fields.kind === "scene" || inspector.entityType !== undefined
                width: parent.width
                spacing: Theme.space.xs

                property bool bad: false

                Caps {
                    text: "Date in the story"
                }

                TextField {
                    objectName: "dateField"
                    width: parent.width
                    Accessible.name: "Date in the story"
                    placeholderText: "Year-month-day, such as 1847-03-04"
                    text: inspector.fields.date ?? ""
                    onEditingFinished: {
                        if (text === (inspector.fields.date ?? ""))
                            return;
                        parent.bad = !inspector.session.setField(inspector.session.currentId, "date", text);
                    }
                }

                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: parent.bad ? "This date does not exist in the calendar of the story."
                                     : (inspector.fields.dateLabel ?? "")
                    visible: text !== ""
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.small
                    color: parent.bad ? Theme.color.accent : Theme.color.inkMuted
                }
            }

            Column {
                objectName: "plotlineSection"
                visible: inspector.fields.kind === "scene"
                width: parent.width
                spacing: Theme.space.xs

                Caps {
                    text: "Plotlines"
                }

                Flow {
                    width: parent.width
                    spacing: Theme.space.xs

                    Repeater {
                        model: inspector.plotlines

                        Choice {
                            required property var modelData

                            objectName: "plotline-" + modelData.id
                            label: modelData.name
                            chosen: (inspector.fields.plotlines ?? []).includes(modelData.id)
                            onClicked: inspector.togglePlotline(modelData.id)
                        }
                    }
                }

                TextField {
                    objectName: "newPlotline"
                    width: parent.width
                    Accessible.name: "New plotline"
                    placeholderText: "New plotline, such as \"The romance\""
                    onAccepted: {
                        const id = inspector.session.addPlotline(text);
                        text = "";
                        if (id !== "")
                            inspector.togglePlotline(id);
                    }
                }
            }

            Column {
                id: relationSection

                objectName: "relationSection"
                visible: inspector.entityType !== undefined
                width: parent.width
                spacing: Theme.space.xs

                property bool parentLink: false

                Caps {
                    text: "Relations"
                }

                Repeater {
                    model: inspector.relations

                    Item {
                        id: relationRow

                        required property var modelData

                        width: parent.width
                        height: 30

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - removeRelation.width
                            elide: Text.ElideRight
                            text: relationRow.modelData.fromTitle + " is " + relationRow.modelData.name + " "
                                  + relationRow.modelData.toTitle
                            font.family: Theme.type.ui
                            font.pixelSize: Theme.type.body
                            color: Theme.color.ink
                        }

                        IconButton {
                            id: removeRelation

                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            iconName: "x"
                            label: "Remove the relation " + relationRow.modelData.name
                            onClicked: inspector.session.removeRelation(relationRow.modelData.index)
                        }
                    }
                }

                Row {
                    spacing: Theme.space.xs

                    Choice {
                        label: "Name"
                        chosen: !relationSection.parentLink
                        onClicked: relationSection.parentLink = false
                    }

                    Choice {
                        objectName: "parentLink"
                        label: "Parent of"
                        chosen: relationSection.parentLink
                        onClicked: relationSection.parentLink = true
                    }
                }

                Row {
                    width: parent.width
                    spacing: Theme.space.xs

                    TextField {
                        id: relationName

                        objectName: "relationName"
                        visible: !relationSection.parentLink
                        width: parent.width - linkButton.width - Theme.space.xs
                        Accessible.name: "Name of the relation"
                        placeholderText: "Sister of, rival of"
                    }

                    Button {
                        id: linkButton

                        objectName: "linkButton"
                        text: "Link to"
                        iconName: "chevron-down"
                        iconAfterText: true
                        enabled: relationSection.parentLink || relationName.text.trim() !== ""
                        onClicked: linkMenu.popup(linkButton, 0, linkButton.height)
                    }
                }

                Menu {
                    id: linkMenu

                    objectName: "linkMenu"

                    Instantiator {
                        model: inspector.entityType !== undefined
                               ? inspector.session.items().filter(item => item.id !== inspector.session.currentId
                                   && inspector.session.entityTypes().some(type => type.kind === item.kind)) : []

                        delegate: T.Action {
                            required property var modelData

                            text: modelData.title
                            onTriggered: {
                                if (inspector.session.addRelation(inspector.session.currentId, modelData.id,
                                                                  relationName.text, relationSection.parentLink))
                                    relationName.text = "";
                            }
                        }

                        onObjectAdded: (index, action) => linkMenu.insertAction(index, action as T.Action)
                        onObjectRemoved: (index, action) => linkMenu.removeAction(action as T.Action)
                    }
                }
            }

            Column {
                visible: inspector.entityType !== undefined
                width: parent.width
                spacing: Theme.space.xs

                Caps {
                    text: "Mentions"
                }

                Text {
                    visible: inspector.mentions.length === 0
                    text: "No text of the manuscript names it yet."
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.body
                    color: Theme.color.inkMuted
                }

                Repeater {
                    model: inspector.mentions

                    ListItem {
                        required property var modelData

                        objectName: "mention"
                        width: parent.width
                        text: modelData.title
                        trailing: modelData.count
                        onClicked: inspector.session.currentId = modelData.id
                    }
                }
            }

            // One field for each field of the entity type.
            Repeater {
                model: inspector.entityType ? inspector.entityType.fields : []

                Column {
                    id: typeField

                    required property string modelData

                    width: parent.width
                    spacing: Theme.space.xs

                    Caps {
                        text: typeField.modelData
                    }

                    TextField {
                        width: parent.width
                        Accessible.name: typeField.modelData
                        text: (inspector.fields.values ?? {})[typeField.modelData] ?? ""
                        onEditingFinished: {
                            if (text !== ((inspector.fields.values ?? {})[typeField.modelData] ?? ""))
                                inspector.session.setEntityField(inspector.session.currentId, typeField.modelData, text);
                        }
                    }
                }
            }

            Column {
                width: parent.width
                spacing: Theme.space.xs

                Caps {
                    text: "Tags"
                }

                TextField {
                    width: parent.width
                    placeholderText: "Tags, split by commas"
                    Accessible.name: "Tags"
                    text: (inspector.fields.tags ?? []).join(", ")
                    onEditingFinished: inspector.save("tags", text.split(",").map(t => t.trim()).filter(t => t !== ""))
                }
            }

            Rule {
                width: parent.width
            }

            // Snapshots: copies of the text at a moment, newest first.
            Column {
                id: snapshotSection

                property var list: []

                function refresh() {
                    snapshotSection.list = inspector.session.snapshots(inspector.session.currentId);
                }

                visible: inspector.fields.kind === "scene" || inspector.fields.kind === "page"
                         || inspector.fields.kind === "note"
                width: parent.width
                spacing: Theme.space.s
                Component.onCompleted: refresh()

                Connections {
                    target: inspector.session

                    function onSnapshotsChanged() {
                        snapshotSection.refresh();
                    }

                    function onCurrentIdChanged() {
                        snapshotSection.refresh();
                    }
                }

                Caps {
                    text: "Snapshots"
                }

                Row {
                    width: parent.width
                    spacing: Theme.space.s

                    TextField {
                        id: snapshotTitle

                        width: parent.width - takeButton.width - parent.spacing
                        placeholderText: "Title (optional)"
                        Accessible.name: "Title of the snapshot"
                        onAccepted: takeButton.clicked()
                    }

                    Button {
                        id: takeButton

                        text: "Take"
                        onClicked: {
                            inspector.session.takeSnapshot(inspector.session.currentId, snapshotTitle.text);
                            snapshotTitle.text = "";
                        }
                    }
                }

                Repeater {
                    model: snapshotSection.list

                    Column {
                        id: snapshotRow

                        required property var modelData

                        width: snapshotSection.width
                        spacing: 2

                        Text {
                            width: parent.width
                            text: snapshotRow.modelData.title !== "" ? snapshotRow.modelData.title
                                                                     : snapshotRow.modelData.label
                            elide: Text.ElideRight
                            font.family: Theme.type.ui
                            font.pixelSize: Theme.type.body
                            color: Theme.color.ink
                        }

                        Text {
                            visible: snapshotRow.modelData.title !== ""
                            text: snapshotRow.modelData.label
                            font.family: Theme.type.ui
                            font.pixelSize: Theme.type.small
                            color: Theme.color.inkMuted
                        }

                        Row {
                            spacing: Theme.space.xs
                            leftPadding: -Theme.space.l

                            Button {
                                text: "Compare"
                                font.pixelSize: Theme.type.small + 1
                                onClicked: inspector.compareRequested(snapshotRow.modelData.path,
                                                                      snapshotRow.modelData.label)
                            }

                            Button {
                                text: "Restore"
                                font.pixelSize: Theme.type.small + 1
                                onClicked: inspector.session.restoreSnapshot(inspector.session.currentId,
                                                                             snapshotRow.modelData.path)
                            }
                        }
                    }
                }
            }

            // The saved versions of this text, when the project has versions.
            Column {
                id: versionSection

                property var entries: []

                function refresh() {
                    entries = inspector.session.versionsOn && inspector.session.currentId !== ""
                        ? inspector.session.versions(inspector.session.currentId) : [];
                }

                objectName: "textVersions"
                visible: inspector.session.versionsOn && inspector.entityType === undefined
                         && (inspector.fields.kind === "scene" || inspector.fields.kind === "page"
                             || inspector.fields.kind === "note")
                width: parent.width
                spacing: Theme.space.xs
                Component.onCompleted: refresh()

                Connections {
                    target: inspector.session

                    function onVersionsChanged() {
                        versionSection.refresh();
                    }

                    function onCurrentIdChanged() {
                        versionSection.refresh();
                    }
                }

                Caps {
                    text: "Versions"
                }

                Text {
                    visible: versionSection.entries.length === 0
                    text: "No saved version holds this text yet."
                    font.family: Theme.type.ui
                    font.pixelSize: Theme.type.body
                    color: Theme.color.inkMuted
                }

                Repeater {
                    model: versionSection.entries

                    Column {
                        id: versionRow

                        required property var modelData

                        objectName: "textVersion"
                        width: parent.width

                        Text {
                            width: parent.width
                            text: versionRow.modelData.message
                            elide: Text.ElideRight
                            font.family: Theme.type.ui
                            font.pixelSize: Theme.type.body
                            color: Theme.color.ink
                        }

                        Item {
                            width: parent.width
                            height: compareVersion.height

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: versionRow.modelData.label
                                font.family: Theme.type.ui
                                font.pixelSize: Theme.type.small
                                color: Theme.color.inkMuted
                            }

                            Button {
                                id: compareVersion

                                objectName: "compareVersion"
                                anchors.right: parent.right
                                text: "Compare"
                                onClicked: inspector.compareRequested("version:" + versionRow.modelData.hash,
                                                                      versionRow.modelData.label)
                            }
                        }
                    }
                }
            }

            Rule {
                width: parent.width
            }

            Column {
                width: parent.width
                spacing: Theme.space.xs

                Caps {
                    text: "Notes"
                }

                TextArea {
                    width: parent.width
                    text: inspector.fields.notes ?? ""
                    placeholderText: "Notes for this item"
                    Accessible.name: "Notes"
                    font.family: Theme.type.reading
                    font.pixelSize: Theme.type.large - 1
                    onEditingFinished: inspector.save("notes", text)
                }
            }
        }
    }
}
