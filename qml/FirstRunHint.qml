pragma ComponentBehavior: Bound

import QtQuick

// Three hints on the first open of a project: the binder, the page, and the
// command palette. Escape or a click moves to the next one. After the last
// one, Settings.hintsSeen is true.
Rectangle {
    id: hint

    required property ProjectView view
    // The width of the open binder, or 0.
    property real binderWidth: 0
    property int step: -1
    readonly property var steps: [
        // Each hint sits beside its target, never on a control.
        { text: "The binder, on the left, holds the book. Click a scene to write in it.",
          x: binderWidth + Theme.space.l, y: view.height - height - 3 * Theme.space.xl },
        { text: "Write here. Codicier saves your text as you type.",
          x: binderWidth + (view.width - binderWidth - width) / 2,
          y: view.height - height - 3 * Theme.space.xl },
        { text: (view.keys.sequences["palette"] ?? "") + " finds any command or item. "
                + (view.keys.sequences["help"] ?? "") + " shows all keys.",
          x: view.width - width - Theme.space.l, y: view.height - height - 3 * Theme.space.xl }
    ]

    function next() {
        if (step >= steps.length - 1) {
            step = -1;
            Settings.hintsSeen = true;
        } else {
            step += 1;
        }
    }

    Component.onCompleted: step = Settings.hintsSeen ? -1 : 0
    visible: step >= 0 && !view.focusMode
    z: 20
    x: visible ? steps[step].x : 0
    y: visible ? steps[step].y : 0
    width: 300
    height: hintColumn.implicitHeight + 2 * Theme.space.m
    radius: Theme.radius.medium
    color: Theme.color.surface
    border.width: 1
    border.color: Theme.color.accent
    Accessible.role: Accessible.StaticText
    Accessible.name: visible ? steps[step].text : ""

    Column {
        id: hintColumn

        x: Theme.space.m
        y: Theme.space.m
        width: parent.width - 2 * Theme.space.m
        spacing: Theme.space.xs

        Text {
            width: parent.width
            text: hint.visible ? hint.steps[hint.step].text : ""
            wrapMode: Text.Wrap
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.body
            color: Theme.color.ink
        }

        Text {
            text: (hint.step + 1) + " of " + hint.steps.length + ".  Esc for "
                  + (hint.step === hint.steps.length - 1 ? "done" : "the next hint")
            font.family: Theme.type.ui
            font.pixelSize: Theme.type.small
            color: Theme.color.inkMuted
        }
    }

    TapHandler {
        onTapped: hint.next()
    }
}

