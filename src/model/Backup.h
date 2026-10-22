#pragma once

#include <QDateTime>
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

} // namespace Backup
