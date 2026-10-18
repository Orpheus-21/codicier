#pragma once

#include <QString>

// Zip backups of a project folder.
namespace Backup {

// Writes a zip of the project into <backupFolder>/<project folder name>/,
// without .codicier/, then keeps only the newest keep zips there. Returns
// the path of the zip, or "" with the reason in error.
QString write(const QString &projectFolder, const QString &backupFolder, int keep,
              QString *error = nullptr);

} // namespace Backup
