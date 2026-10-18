import QtQuick
import QtQuick.Shapes

import "IconPaths.js" as IconPaths

// One icon from icons/, drawn as a stroke in any colour.
// Example: Icon { name: "search"; color: Theme.color.inkMuted }
Item {
    id: icon

    required property string name
    property color color: Theme.color.ink
    property real size: 20
    // The path data, or an empty string if no icon has this name.
    readonly property string path: IconPaths.paths[name] ?? ""

    implicitWidth: size
    implicitHeight: size

    Shape {
        // The icons are drawn on a grid of 24 units.
        width: 24
        height: 24
        scale: icon.size / 24
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: icon.color
            strokeWidth: 2
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: icon.path
            }
        }
    }
}
