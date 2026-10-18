pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

// Panes side by side with a thin rule between them. The rule is 1 pixel, but
// the handle that the mouse grabs is 7 pixels wide. The rule sits on the
// side of the handle that faces the outer pane, so the rest of the handle
// has the colour of the middle pane and no strip shows.
T.SplitView {
    id: control

    handle: Item {
        id: handle

        readonly property bool horizontal: control.orientation === Qt.Horizontal
        // ponytail: decides the side by the half of the view that the handle
        // is in; a pane list that knows its edges can replace this.
        readonly property bool atEnd: horizontal ? x + width / 2 > control.width / 2
                                                 : y + height / 2 > control.height / 2

        implicitWidth: 7
        implicitHeight: 7

        Rectangle {
            x: handle.horizontal && handle.atEnd ? handle.width - 1 : 0
            y: !handle.horizontal && handle.atEnd ? handle.height - 1 : 0
            width: handle.horizontal ? 1 : handle.width
            height: handle.horizontal ? handle.height : 1
            color: handle.T.SplitHandle.pressed || handle.T.SplitHandle.hovered
                   ? Theme.color.accent : Theme.color.rule
        }
    }
}
