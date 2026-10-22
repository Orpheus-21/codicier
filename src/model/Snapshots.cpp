#include "Snapshots.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <algorithm>

namespace {

// "2026-10-18T19-30-00": a time stamp that a file name can hold everywhere.
QString stampFormat()
{
    return QStringLiteral("yyyy-MM-ddTHH-mm-ss");
}

constexpr qsizetype stampLength = 19;

bool readAll(const QString &path, QByteArray *bytes)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    *bytes = file.readAll();
    return true;
}

bool writeAll(const QString &path, const QByteArray &bytes)
{
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

// A title that any file system can hold: no slash, no control character,
// and no dot or space at the end.
QString cleanTitle(const QString &title)
{
    QString clean;
    for (const QChar c : title) {
        if (c.category() == QChar::Other_Control || QStringLiteral("/\\\\:*?\"<>|").contains(c))
            clean += u' ';
        else
            clean += c;
    }
    clean = clean.simplified().left(80);
    while (clean.endsWith(u'.') || clean.endsWith(u' '))
        clean.chop(1);
    return clean;
}

} // namespace

namespace Snapshots {

QString take(const QString &projectFolder, const QString &itemId, const QString &file,
             const QString &title)
{
    const QDir dir(projectFolder);
    QByteArray bytes;
    if (itemId.isEmpty() || file.isEmpty() || !readAll(dir.filePath(file), &bytes))
        return { };
    return keep(projectFolder, itemId, title, bytes);
}

QString keep(const QString &projectFolder, const QString &itemId, const QString &title,
             const QByteArray &bytes)
{
    const QDir dir(projectFolder);
    if (itemId.isEmpty())
        return { };
    const QString folder = QStringLiteral("snapshots/") + itemId;
    if (!dir.mkpath(folder))
        return { };
    const QString clean = cleanTitle(title);
    const QString base = QDateTime::currentDateTime().toString(stampFormat())
        + (clean.isEmpty() ? QString() : QStringLiteral(" ") + clean);
    // Two snapshots in the same second get "-2", "-3" after the stamp.
    QString name = base + QStringLiteral(".md");
    for (int n = 2; QFileInfo::exists(dir.filePath(folder + u'/' + name)); ++n)
        name = base.left(stampLength) + QStringLiteral("-%1").arg(n) + base.mid(stampLength)
            + QStringLiteral(".md");
    const QString path = folder + u'/' + name;
    return writeAll(dir.filePath(path), bytes) ? path : QString();
}

QList<Snapshot> list(const QString &projectFolder, const QString &itemId)
{
    QList<Snapshot> snapshots;
    const QString folder = QStringLiteral("snapshots/") + itemId;
    const QDir dir(QDir(projectFolder).filePath(folder));
    for (const QFileInfo &info : dir.entryInfoList({ QStringLiteral("*.md") }, QDir::Files)) {
        const QString name = info.completeBaseName();
        const QDateTime time = QDateTime::fromString(name.left(stampLength), stampFormat());
        if (!time.isValid())
            continue;
        // After the stamp: an optional "-2", then an optional " title".
        QString rest = name.mid(stampLength);
        const qsizetype space = rest.indexOf(u' ');
        const QString title = space < 0 ? QString() : rest.mid(space + 1);
        snapshots.append({ folder + u'/' + info.fileName(), time, title });
    }
    std::ranges::sort(snapshots, [](const Snapshot &a, const Snapshot &b) {
        return a.time != b.time ? a.time > b.time : a.path > b.path;
    });
    return snapshots;
}

bool restore(const QString &projectFolder, const QString &itemId, const QString &file,
             const QString &snapshotPath)
{
    const QDir dir(projectFolder);
    // Only a snapshot of this item can restore it.
    if (!snapshotPath.startsWith(QStringLiteral("snapshots/") + itemId + u'/')
        || snapshotPath.contains(QStringLiteral("..")))
        return false;
    QByteArray bytes;
    if (!readAll(dir.filePath(snapshotPath), &bytes))
        return false;
    if (take(projectFolder, itemId, file, QStringLiteral("Before restore")).isEmpty())
        return false;
    return writeAll(dir.filePath(file), bytes);
}

} // namespace Snapshots
