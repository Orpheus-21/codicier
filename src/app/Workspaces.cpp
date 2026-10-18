#include "Workspaces.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {

constexpr auto currentKey = "workspace/current";
constexpr auto recentKey = "workspace/recent";

} // namespace

Workspaces::Workspaces(QObject *parent) : QObject(parent)
{
    const QSettings settings(settingsPath(), QSettings::IniFormat);
    currentFolder = settings.value(currentKey).toString();
    recentFolders = settings.value(recentKey).toStringList();
}

QString Workspaces::settingsPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
        .filePath(QStringLiteral("settings.ini"));
}

QString Workspaces::open(const QString &folder)
{
    const QFileInfo info(folder);
    if (folder.isEmpty() || !info.exists())
        return QStringLiteral("The folder does not exist.");
    if (!info.isDir())
        return QStringLiteral("This is a file, not a folder.");
    if (!info.isWritable())
        return QStringLiteral("The app cannot write in this folder.");

    const QString path = QDir::cleanPath(info.absoluteFilePath());
    currentFolder = path;
    recentFolders.removeAll(path);
    recentFolders.prepend(path);
    while (recentFolders.size() > recentLimit)
        recentFolders.removeLast();
    store();
    emit changed();
    return { };
}

QString Workspaces::openUrl(const QUrl &url)
{
    return open(url.toLocalFile());
}

void Workspaces::forget(const QString &folder)
{
    if (recentFolders.removeAll(folder) == 0 && folder != currentFolder)
        return;
    if (folder == currentFolder)
        currentFolder.clear();
    store();
    emit changed();
}

QString Workspaces::nameOf(const QString &folder)
{
    return QFileInfo(folder).fileName();
}

void Workspaces::store() const
{
    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.setValue(currentKey, currentFolder);
    settings.setValue(recentKey, recentFolders);
}
