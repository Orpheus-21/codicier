#pragma once

#include <QList>
#include <QString>

#include <cstdint>

// The word-level difference between two texts.
namespace Diff {

struct Segment
{
    enum Kind : std::uint8_t { Same, Added, Removed };
    Kind kind = Same;
    QString text;

    bool operator==(const Segment &) const = default;
};

// The segments that turn before into after, in text order. Joining the Same
// and Removed segments gives before; joining Same and Added gives after.
QList<Segment> words(const QString &before, const QString &after);

// For each character of the last version, the index of the first version
// that has it, from oldest to newest. A character of the first version has
// 0. The app uses it to color the text by the draft pass that wrote it.
QList<int> origins(const QStringList &versions);

} // namespace Diff
