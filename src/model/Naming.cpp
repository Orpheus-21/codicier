#include "Naming.h"

#include <QList>
#include <QRandomGenerator>

#include <algorithm>

namespace {

constexpr char idAlphabet[] = "0123456789abcdefghjkmnpqrstvwxyz";
constexpr int idLength = 10;
constexpr int nameLength = 60;

} // namespace

namespace Naming {

QString newItemId(const QSet<QString> &used)
{
    QString id;
    do {
        id.clear();
        for (int i = 0; i < idLength; ++i)
            id += QLatin1Char(idAlphabet[QRandomGenerator::system()->bounded(32)]);
    } while (used.contains(id));
    return id;
}

QString fileNameFor(const QString &title, const QSet<QString> &taken)
{
    // Each run of characters that are not letters or digits becomes one
    // hyphen. The loop reads whole code points, so a letter outside the
    // basic plane stays whole.
    QList<char32_t> points;
    for (const char32_t c : title.toLower().toUcs4()) {
        if (QChar::isLetterOrNumber(c))
            points.append(c);
        else if (!points.isEmpty() && points.back() != U'-')
            points.append(U'-');
    }
    points = points.first(std::min<qsizetype>(points.size(), nameLength));
    while (!points.isEmpty() && points.back() == U'-')
        points.removeLast();
    QString base = QString::fromUcs4(points.constData(), points.size());
    if (base.isEmpty())
        base = QStringLiteral("untitled");

    QString name = base + QStringLiteral(".md");
    for (int n = 2; taken.contains(name); ++n)
        name = QStringLiteral("%1-%2.md").arg(base).arg(n);
    return name;
}

} // namespace Naming
