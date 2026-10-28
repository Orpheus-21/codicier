#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

// Zip backups of a project folder.
namespace Backup {

// Writes a zip of the project into <backupFolder>/<project folder name>/,
// without .codicier/, then keeps only the newest keep zips there. Returns
// the path of the zip, or "" with the reason in error.
QString write(const QString &projectFolder, const QString &backupFolder, int keep,
              QString *error = nullptr);

// Deletes every zip of the project in <backupFolder>/<project folder name>/,
// and the folder when it is then empty. A zip is of the project when its
// project.toml has the same created time. The zips of another project with
// the same folder name stay. Returns the number of deleted zips, or -1 with
// the reason in error.
int removeAll(const QString &projectFolder, const QString &backupFolder, QString *error = nullptr);

// The time of the newest zip of the project in the backup folder, or an
// invalid time when it has none.
QDateTime latest(const QString &projectFolder, const QString &backupFolder);

// One zip backup of a project.
struct Entry
{
    QString path;
    QDateTime time;
};

// The zips of the project in the backup folder, newest first. A zip is of
// the project when its project.toml has the same created time.
QList<Entry> list(const QString &projectFolder, const QString &backupFolder);

// Unpacks a zip into targetFolder, which must not exist yet. The folder name
// at the top of the zip is dropped. Returns false with the reason in error;
// then the caller removes the folder.
bool restore(const QString &zipPath, const QString &targetFolder, QString *error = nullptr);

// Writes a zip of the project, without .codicier/, to zipFile. Returns false
// with the reason in error.
bool exportTo(const QString &projectFolder, const QString &zipFile, QString *error = nullptr);

} // namespace Backup
