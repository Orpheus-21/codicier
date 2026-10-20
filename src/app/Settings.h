#pragma once

#include "Workspaces.h"

#include <QObject>

#include <QSettings>
#include <QString>
#include <QtQml/qqmlregistration.h>
#include <algorithm>

// The settings of the app that are not about one workspace, in the same
// settings.ini as the workspaces.
class Settings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // The folder for the zip backups of the projects.
    Q_PROPERTY(QString backupFolder READ backupFolder WRITE setBackupFolder NOTIFY changed)
    // The number of backups to keep for each project, 1 to 1000.
    Q_PROPERTY(int backupsToKeep READ backupsToKeep WRITE setBackupsToKeep NOTIFY changed)
    // "system", "light", "dark", or "sepia".
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY changed)
    // A font family, or "" for the font that comes with the app.
    Q_PROPERTY(QString readingFont READ readingFont WRITE setReadingFont NOTIFY changed)
    Q_PROPERTY(QString uiFont READ uiFont WRITE setUiFont NOTIFY changed)
    // The size of the text on the page in pixels, 14 to 28.
    Q_PROPERTY(int readingSize READ readingSize WRITE setReadingSize NOTIFY changed)
    // The width of the page in characters, 45 to 90.
    Q_PROPERTY(int measure READ measure WRITE setMeasure NOTIFY changed)
    // The wait before an autosave in milliseconds, 250 to 10000.
    Q_PROPERTY(int autosaveDelay READ autosaveDelay WRITE setAutosaveDelay NOTIFY changed)
    // True turns every animation off.
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY changed)
    // The word target of one writing session, 0 for none, up to 100000.
    Q_PROPERTY(int sessionTarget READ sessionTarget WRITE setSessionTarget NOTIFY changed)
    // The length of a writing sprint in minutes, 1 to 240.
    Q_PROPERTY(int sprintMinutes READ sprintMinutes WRITE setSprintMinutes NOTIFY changed)
    // True underlines the words that the dictionary does not know.
    Q_PROPERTY(bool spellCheck READ spellCheck WRITE setSpellCheck NOTIFY changed)
    // The style checks that are on: 1 repeated words, 2 filter words,
    // 4 adverbs, and 8 passive voice. 0 turns all off.
    Q_PROPERTY(int styleChecks READ styleChecks WRITE setStyleChecks NOTIFY changed)
    // True saves a version when a project with versions closes.
    Q_PROPERTY(bool versionOnClose READ versionOnClose WRITE setVersionOnClose NOTIFY changed)

public:
    static constexpr int defaultBackupsToKeep = 10;

    explicit Settings(QObject *parent = nullptr);

    static QString defaultBackupFolder();

    QString backupFolder() const { return folder; }
    void setBackupFolder(const QString &value);
    int backupsToKeep() const { return keep; }
    void setBackupsToKeep(int value);
    QString theme() const { return themeName; }
    void setTheme(const QString &value);
    QString readingFont() const { return readingFamily; }
    void setReadingFont(const QString &value) { change(readingFamily, value, "view/readingFont"); }
    QString uiFont() const { return uiFamily; }
    void setUiFont(const QString &value) { change(uiFamily, value, "view/uiFont"); }
    int readingSize() const { return size; }
    void setReadingSize(int value);
    int measure() const { return columns; }
    void setMeasure(int value);
    int autosaveDelay() const { return delay; }
    void setAutosaveDelay(int value);
    bool reducedMotion() const { return lessMotion; }
    void setReducedMotion(bool value) { change(lessMotion, value, "view/reducedMotion"); }
    int sessionTarget() const { return target; }
    void setSessionTarget(int value);
    int sprintMinutes() const { return sprint; }
    bool spellCheck() const { return spelling; }
    int styleChecks() const { return style; }
    bool versionOnClose() const { return closeVersion; }
    void setVersionOnClose(bool value) { change(closeVersion, value, "versions/onClose"); }
    void setStyleChecks(int value)
    {
        change(style, std::clamp(value, 0, 255) % 16, "edit/styleChecks");
    }
    void setSpellCheck(bool value) { change(spelling, value, "edit/spellCheck"); }
    void setSprintMinutes(int value);
    // The same for a URL from a folder dialog.
    Q_INVOKABLE void setBackupFolderUrl(const QUrl &url);

signals:
    void changed();

private:
    QString folder;
    int keep = defaultBackupsToKeep;
    QString themeName = QStringLiteral("system");
    QString readingFamily;
    QString uiFamily;
    int size = 19;
    int columns = 66;
    int delay = 1000;
    bool lessMotion = false;
    int target = 0;
    int sprint = 25;
    bool spelling = true;
    int style = 0;
    bool closeVersion = true;

    // Sets a value, writes it to the settings file, and tells the views.
    template <typename T> void change(T &member, const T &value, const char *key)
    {
        if (member == value)
            return;
        member = value;
        QSettings(Workspaces::settingsPath(), QSettings::IniFormat).setValue(key, value);
        emit changed();
    }

    void store() const;
};
