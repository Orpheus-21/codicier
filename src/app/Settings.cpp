#include "Settings.h"

#include "Document.h"
#include "SpellChecker.h"
#include "Workspaces.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTime>
#include <QUrl>

#include <algorithm>
#include <array>

namespace {

constexpr auto folderKey = "backup/folder";
constexpr auto keepKey = "backup/keep";
constexpr int maxKeep = 1000;
constexpr std::array<QStringView, 7> themes { u"system", u"light", u"dark",    u"sepia",
                                              u"night",  u"mist",  u"contrast" };
// "theme" keeps the accent of the theme. Theme.qml holds the colours.
constexpr std::array<QStringView, 5> accents { u"theme", u"blue", u"green", u"plum", u"red" };

template <std::size_t N> bool isOneOf(const std::array<QStringView, N> &names, const QString &name)
{
    return std::ranges::find(names, QStringView(name)) != names.end();
}

} // namespace

void Settings::setReminderTime(const QString &value)
{
    const QString time = value.trimmed();
    if (time.isEmpty() || QTime::fromString(time, QStringLiteral("HH:mm")).isValid())
        change(reminderAt, time, "goals/reminderTime");
}

QStringList Settings::dictionaries()
{
    return SpellChecker::installed();
}

Settings::Settings(QObject *parent) : QObject(parent)
{
    const QSettings settings(Workspaces::settingsPath(), QSettings::IniFormat);
    folder = settings.value(folderKey, defaultBackupFolder()).toString();
    keep = std::clamp(settings.value(keepKey, defaultBackupsToKeep).toInt(), 1, maxKeep);
    themeName = settings.value("view/theme", themeName).toString();
    if (!isOneOf(themes, themeName))
        themeName = QStringLiteral("system");
    accentName = settings.value("view/accent", accentName).toString();
    if (!isOneOf(accents, accentName))
        accentName = QStringLiteral("theme");
    readingFamily = settings.value("view/readingFont").toString();
    uiFamily = settings.value("view/uiFont").toString();
    size = std::clamp(settings.value("view/readingSize", size).toInt(), 14, 28);
    scale = std::clamp(settings.value("view/uiScale", scale).toInt(), 100, 130);
    columns = std::clamp(settings.value("view/measure", columns).toInt(), 45, 90);
    delay = std::clamp(settings.value("edit/autosaveDelay", delay).toInt(), 250, 10000);
    lessMotion = settings.value("view/reducedMotion", false).toBool();
    calm = settings.value("view/calmFocus", true).toBool();
    essayTools = settings.value("view/essayTools", false).toBool();
    labels = settings.value("view/buttonLabels", false).toBool();
    seen = settings.value("view/hintsSeen", false).toBool();
    marksHidden = settings.value("view/hideMarks", true).toBool();
    free = settings.value("view/freeBoard", false).toBool();
    reminderAt = settings.value("goals/reminderTime").toString();
    if (!QTime::fromString(reminderAt, QStringLiteral("HH:mm")).isValid())
        reminderAt.clear();
    shownOn = settings.value("goals/reminderShown").toString();
    cards = settings.value("view/cardColour", cards).toString() == QLatin1String("pov")
        ? QStringLiteral("pov")
        : QStringLiteral("label");
    scope = std::clamp(settings.value("view/focusScope", scope).toInt(), 0, 2);
    spacing = std::clamp(settings.value("view/lineSpacing", spacing).toInt(), 120, 200);
    paragraphSpace
        = std::clamp(settings.value("view/paragraphGap", paragraphSpace).toInt(), 0, 100);
    target = std::clamp(settings.value("goals/sessionTarget", 0).toInt(), 0, 100000);
    sprint = std::clamp(settings.value("goals/sprintMinutes", sprint).toInt(), 1, 240);
    spelling = settings.value("edit/spellCheck", true).toBool();
    style = std::clamp(settings.value("edit/styleChecks", 0).toInt(), 0, 255) % 16;
    smart = std::clamp(settings.value("edit/typography", smart).toInt(), 0, 7);
    closeVersion = settings.value("versions/onClose", true).toBool();
    Document::saveDelayMs = delay;
}

void Settings::setTheme(const QString &value)
{
    if (isOneOf(themes, value))
        change(themeName, value, "view/theme");
}

void Settings::setAccent(const QString &value)
{
    if (isOneOf(accents, value))
        change(accentName, value, "view/accent");
}

void Settings::setSessionTarget(int value)
{
    change(target, std::clamp(value, 0, 100000), "goals/sessionTarget");
}

void Settings::setSprintMinutes(int value)
{
    change(sprint, std::clamp(value, 1, 240), "goals/sprintMinutes");
}

void Settings::setReadingSize(int value)
{
    change(size, std::clamp(value, 14, 28), "view/readingSize");
}

void Settings::setUiScale(int value)
{
    change(scale, std::clamp(value, 100, 130), "view/uiScale");
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
