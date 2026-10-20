#include "Settings.h"

#include "Document.h"
#include "Workspaces.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>
#include <array>

namespace {

constexpr auto folderKey = "backup/folder";
constexpr auto keepKey = "backup/keep";
constexpr int maxKeep = 1000;
constexpr std::array<QStringView, 4> themes { u"system", u"light", u"dark", u"sepia" };

bool isTheme(const QString &name)
{
    return std::ranges::find(themes, QStringView(name)) != themes.end();
}

} // namespace

Settings::Settings(QObject *parent) : QObject(parent)
{
    const QSettings settings(Workspaces::settingsPath(), QSettings::IniFormat);
    folder = settings.value(folderKey, defaultBackupFolder()).toString();
    keep = std::clamp(settings.value(keepKey, defaultBackupsToKeep).toInt(), 1, maxKeep);
    themeName = settings.value("view/theme", themeName).toString();
    if (!isTheme(themeName))
        themeName = QStringLiteral("system");
    readingFamily = settings.value("view/readingFont").toString();
    uiFamily = settings.value("view/uiFont").toString();
    size = std::clamp(settings.value("view/readingSize", size).toInt(), 14, 28);
    columns = std::clamp(settings.value("view/measure", columns).toInt(), 45, 90);
    delay = std::clamp(settings.value("edit/autosaveDelay", delay).toInt(), 250, 10000);
    lessMotion = settings.value("view/reducedMotion", false).toBool();
    target = std::clamp(settings.value("goals/sessionTarget", 0).toInt(), 0, 100000);
    Document::saveDelayMs = delay;
}

void Settings::setTheme(const QString &value)
{
    if (isTheme(value))
        change(themeName, value, "view/theme");
}

void Settings::setSessionTarget(int value)
{
    change(target, std::clamp(value, 0, 100000), "goals/sessionTarget");
}

void Settings::setReadingSize(int value)
{
    change(size, std::clamp(value, 14, 28), "view/readingSize");
}

void Settings::setMeasure(int value)
{
    change(columns, std::clamp(value, 45, 90), "view/measure");
}

void Settings::setAutosaveDelay(int value)
{
    change(delay, std::clamp(value, 250, 10000), "edit/autosaveDelay");
    Document::saveDelayMs = delay;
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
