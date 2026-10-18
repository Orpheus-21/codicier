pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Panes side by side with a thin rule between them. The rule is 1 pixel, but
// the handle that the mouse grabs is 7 pixels wide.
T.SplitView {
    id: control

    handle: Item {
        implicitWidth: 7
        implicitHeight: 7

        Rectangle {
            anchors.centerIn: parent
            width: control.orientation === Qt.Horizontal ? 1 : parent.width
            height: control.orientation === Qt.Horizontal ? parent.height : 1
            color: parent.T.SplitHandle.pressed || parent.T.SplitHandle.hovered
                   ? Theme.color.accent : Theme.color.rule
        }
    }
}
