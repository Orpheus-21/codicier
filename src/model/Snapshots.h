#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

// Copies of a text item at a moment, in snapshots/<item id>/ of the project.
// Each copy is a plain Markdown file named "<date and time> <title>.md".
namespace Snapshots {

struct Snapshot
{
    // The path relative to the project folder.
    QString path;
    QDateTime time;
    QString title;
};

// Copies the file of the item. Returns the path of the copy, or "" when the
// file cannot be read or the copy cannot be written.
QString take(const QString &projectFolder, const QString &itemId, const QString &file,
             const QString &title);

// Keeps bytes as a snapshot of the item, as take does with its file.
QString keep(const QString &projectFolder, const QString &itemId, const QString &title,
             const QByteArray &bytes);

// The snapshots of the item, newest first.
QList<Snapshot> list(const QString &projectFolder, const QString &itemId);

// Writes the snapshot back to the file of the item. The current text first
// becomes a snapshot with the title "Before restore", so no text is lost.
// Returns false if any step fails; the file then stays as it was.
bool restore(const QString &projectFolder, const QString &itemId, const QString &file,
             const QString &snapshotPath);

} // namespace Snapshots
