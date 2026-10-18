#pragma once

#include <QSet>
#include <QString>

// The ids and file names of docs/format.md.
namespace Naming {

// A random id of 10 characters that is not in used.
QString newItemId(const QSet<QString> &used);

// A file name for a new item, made from its title, that is not in taken.
// Example: "A Date She Did Not Know" gives "a-date-she-did-not-know.md".
QString fileNameFor(const QString &title, const QSet<QString> &taken);

} // namespace Naming
