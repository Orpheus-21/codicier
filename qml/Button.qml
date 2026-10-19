import QtQuick
import QtQuick.Templates as T

// A text button. "primary" fills with the accent colour and is for the one
// main action of a screen. "quiet" is text only.
T.Button {
    id: control

    // "primary" or "quiet".
    property string kind: "quiet"
    property string iconName: ""
    // Puts the icon after the text, as for a menu arrow.
    property bool iconAfterText: false
    // The screen reader name. Qt fills it from the text only while a screen
    // reader runs, so the button sets it.
    Accessible.name: text
    readonly property color foreground: kind === "primary" ? Theme.color.paper : Theme.color.ink

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    topPadding: Theme.space.s
    bottomPadding: Theme.space.s
    leftPadding: Theme.space.l
    rightPadding: Theme.space.l
    spacing: Theme.space.s
    font.family: Theme.type.ui
    font.pixelSize: Theme.type.body
    font.weight: Font.Medium

    contentItem: Row {
        spacing: control.spacing
        layoutDirection: control.iconAfterText ? Qt.RightToLeft : Qt.LeftToRight

        Icon {
            visible: control.iconName !== ""
            anchors.verticalCenter: parent.verticalCenter
            name: control.iconName
            size: 16
            color: control.foreground
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            font: control.font
            color: control.foreground
        }
    }

    background: Rectangle {
        implicitHeight: 40
        radius: Theme.radius.small
        color: {
            if (control.kind === "primary")
                return Theme.color.accent;
            return control.hovered || control.down ? Theme.color.selection : "transparent";
        }
        opacity: !control.enabled ? 0.5 : control.down && control.kind === "primary" ? 0.85 : 1

        FocusRing {
            visible: control.visualFocus
        }
    }
}
