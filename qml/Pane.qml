import QtQuick
import QtQuick.Templates as T

// A side panel, such as the binder or the inspector.
T.Pane {
    // The template has no size of its own, so take the content plus padding.
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)
    padding: Theme.space.l

    background: Rectangle {
        color: Theme.color.surface
    }
}
