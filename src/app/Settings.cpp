#include "Settings.h"

#include "Workspaces.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>

namespace {

constexpr auto folderKey = "backup/folder";
constexpr auto keepKey = "backup/keep";
constexpr int maxKeep = 1000;

} // namespace

Settings::Settings(QObject *parent) : QObject(parent)
{
    const QSettings settings(Workspaces::settingsPath(), QSettings::IniFormat);
    folder = settings.value(folderKey, defaultBackupFolder()).toString();
    keep = std::clamp(settings.value(keepKey, defaultBackupsToKeep).toInt(), 1, maxKeep);
}

QString Settings::defaultBackupFolder()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("backups"));
}

void Settings::setBackupFolder(const QString &value)
{
    const QString clean = value.isEmpty() ? QString() : QDir::cleanPath(value);
    if (clean == folder)
        return;
    folder = clean;
    store();
    emit changed();
}

void Settings::setBackupFolderUrl(const QUrl &url)
{
    setBackupFolder(url.toLocalFile());
}

void Settings::setBackupsToKeep(int value)
{
    const int clamped = std::clamp(value, 1, maxKeep);
    if (clamped == keep)
        return;
    keep = clamped;
    store();
    emit changed();
}

void Settings::store() const
{
    QSettings settings(Workspaces::settingsPath(), QSettings::IniFormat);
    settings.setValue(folderKey, folder);
    settings.setValue(keepKey, keep);
}
