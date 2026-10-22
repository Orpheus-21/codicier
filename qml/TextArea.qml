import QtQuick
import QtQuick.Templates as T

// A text field of more than one line, such as a synopsis or notes.
T.TextArea {
    id: control

    // The screen reader name. A field with a label above it sets the label.
    Accessible.name: placeholderText
    implicitWidth: 240
    implicitHeight: Math.max(72, contentHeight + topPadding + bottomPadding)
    padding: Theme.space.s
    wrapMode: TextEdit.Wrap
    font.family: Theme.type.ui
    font.pixelSize: Theme.type.body
    color: Theme.color.ink
    placeholderTextColor: Theme.color.inkMuted
    selectionColor: Theme.color.selection
    selectedTextColor: Theme.color.ink
    // Tab and Shift+Tab move to the next control, as in a one-line field.
    // A form field has no use for a tab character.
    Keys.onTabPressed: control.nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocusReason)
    Keys.onBacktabPressed: control.nextItemInFocusChain(false).forceActiveFocus(Qt.BacktabFocusReason)

    background: Rectangle {
        radius: Theme.radius.small
        color: Theme.color.paper
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.color.focus : Theme.color.rule

        // The template draws no placeholder, so draw it here.
        Text {
            x: control.leftPadding
            y: control.topPadding
            width: parent.width - control.leftPadding - control.rightPadding
            visible: control.length === 0 && control.preeditText === ""
            text: control.placeholderText
            wrapMode: Text.Wrap
            font: control.font
            color: control.placeholderTextColor
        }
    }
}
