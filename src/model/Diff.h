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

} // namespace Diff
