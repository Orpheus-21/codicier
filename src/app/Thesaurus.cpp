#include "Thesaurus.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

QStringList &Thesaurus::searchPaths()
{
    // /app/share/mythes holds the thesaurus of the Flatpak.
    static QStringList paths { QStringLiteral("/usr/share/mythes"),
                               QStringLiteral("/app/share/mythes"),
                               QStringLiteral("/usr/share/myspell/dicts") };
    return paths;
}

Thesaurus::Thesaurus(const QString &language)
{
    QString wanted = language;
    wanted.replace(u'-', u'_');
    const QString prefix = wanted.section(u'_', 0, 0);
    // The exact language first, then any file of the same language.
    for (const QString &pattern :
         { QStringLiteral("th_%1*.dat").arg(wanted), QStringLiteral("th_%1_*.dat").arg(prefix) }) {
        for (const QString &path : std::as_const(searchPaths())) {
            const QDir dir(path);
            for (const QString &dat : dir.entryList({ pattern }, QDir::Files, QDir::Name)) {
                // The index loads at the first lookup, not when a project opens.
                data = dir.filePath(dat);
                index = dir.filePath(QFileInfo(dat).completeBaseName() + QStringLiteral(".idx"));
                return;
            }
        }
    }
}

void Thesaurus::load() const
{
    loaded = true;
    QFile idx(index);
    if (idx.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // The first line names the encoding, and the second the count.
        utf8 = idx.readLine().trimmed().toUpper().startsWith("UTF");
        idx.readLine();
        while (!idx.atEnd()) {
            const QString line = decode(idx.readLine().trimmed());
            const qsizetype bar = line.lastIndexOf(u'|');
            if (bar > 0)
                offsets.insert(line.left(bar).toLower(), line.mid(bar + 1).toLongLong());
        }
    } else {
        // LibreOffice builds the index when it installs the thesaurus. Without
        // one, read the entries: "word|count", then count lines of meanings.
        QFile file(data);
        if (!file.open(QIODevice::ReadOnly))
            return;
        utf8 = file.readLine().trimmed().toUpper().startsWith("UTF");
        while (!file.atEnd()) {
            const qint64 at = file.pos();
            const QString head = decode(file.readLine().trimmed());
            const int count = head.section(u'|', 1, 1).toInt();
            offsets.insert(head.section(u'|', 0, 0).toLower(), at);
            for (int i = 0; i < count && !file.atEnd(); ++i)
                file.readLine();
        }
    }
}

QString Thesaurus::decode(const QByteArray &line) const
{
    return utf8 ? QString::fromUtf8(line) : QString::fromLatin1(line);
}

QStringList Thesaurus::synonyms(const QString &word) const
{
    if (!loaded)
        load();
    const QString key = word.toLower();
    const auto found = offsets.constFind(key);
    if (found == offsets.constEnd())
        return { };
    QFile file(data);
    if (!file.open(QIODevice::ReadOnly) || !file.seek(found.value()))
        return { };
    // The entry: "word|count", then count lines of "(part)|synonym|synonym".
    const QString head = decode(file.readLine().trimmed());
    const int count = head.section(u'|', 1, 1).toInt();
    // A synonym can carry a note. A plain word is the best match, and a
    // "similar term" comes after. A "generic term" is broader and an
    // "antonym" is the opposite, so they do not count.
    static const QRegularExpression note(QStringLiteral("\\s*\\(([^)]*)\\)$"));
    QStringList words;
    QStringList similar;
    for (int i = 0; i < count && !file.atEnd(); ++i) {
        const QStringList parts = decode(file.readLine().trimmed()).split(u'|');
        for (qsizetype p = 1; p < parts.size(); ++p) {
            QString synonym = parts.at(p);
            const QRegularExpressionMatch match = note.match(synonym);
            QStringList *list = &words;
            if (match.hasMatch()) {
                if (match.captured(1) != QLatin1String("similar term"))
                    continue;
                synonym.truncate(match.capturedStart());
                list = &similar;
            }
            if (!synonym.isEmpty() && synonym.toLower() != key && !words.contains(synonym)
                && !similar.contains(synonym))
                list->append(synonym);
        }
    }
    words.append(similar);
    return words;
}
