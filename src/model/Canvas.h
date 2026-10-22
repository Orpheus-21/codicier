#pragma once

#include <QString>
#include <QVariantMap>

// The file of an idea canvas, in canvases/: TOML with a [[node]] table for
// each note or card and an [[edge]] table for each line between two nodes.
// A node has an id, a place (x and y), and a text for a note or an item id
// for a card. An edge has the ids of its two nodes (from and to).
namespace Canvas {

// Reads a canvas as {nodes: [{id, x, y, text, item}], edges: [{from, to}]}.
// A missing or broken file gives an empty canvas.
QVariantMap read(const QString &path);

// Writes a canvas in the form that read gives. Returns an error text, or "".
QString write(const QString &path, const QVariantMap &canvas);

} // namespace Canvas
