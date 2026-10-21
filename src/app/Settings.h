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
    // The line height of the page in percent of the text size: 130, 150, or
    // 180.
    Q_PROPERTY(int lineSpacing READ lineSpacing WRITE setLineSpacing NOTIFY changed)
    // The blank line between two paragraphs, in percent of a line: 50 or
    // 100. 0 hides the blank lines and indents the first line instead.
    Q_PROPERTY(int paragraphGap READ paragraphGap WRITE setParagraphGap NOTIFY changed)
    // What stays bright in focus mode: 0 all the text, 1 the paragraph with
    // the cursor, 2 its sentence.
    Q_PROPERTY(int focusScope READ focusScope WRITE setFocusScope NOTIFY changed)
    // True hides the Markdown marks outside the paragraph with the cursor.
    Q_PROPERTY(bool hideMarks READ hideMarks WRITE setHideMarks NOTIFY changed)
    // True after the author saw the first-run hints of a project.
    Q_PROPERTY(bool hintsSeen READ hintsSeen WRITE setHintsSeen NOTIFY changed)
    // True shows a text label next to the icons of the top bar.
    Q_PROPERTY(bool buttonLabels READ buttonLabels WRITE setButtonLabels NOTIFY changed)
    // The word target of one writing session, 0 for none, up to 100000.
    Q_PROPERTY(int sessionTarget READ sessionTarget WRITE setSessionTarget NOTIFY changed)
    // The length of a writing sprint in minutes, 1 to 240.
    Q_PROPERTY(int sprintMinutes READ sprintMinutes WRITE setSprintMinutes NOTIFY changed)
    // True underlines the words that the dictionary does not know.
    Q_PROPERTY(bool spellCheck READ spellCheck WRITE setSpellCheck NOTIFY changed)
    // The style checks that are on: 1 repeated words, 2 filter words,
    // 4 adverbs, and 8 passive voice. 0 turns all off.
    Q_PROPERTY(int styleChecks READ styleChecks WRITE setStyleChecks NOTIFY changed)
    // The changes as the author types: 1 curly quotes, 2 an em dash for
    // "--", and 4 an ellipsis for "...". All are on by default.
    Q_PROPERTY(int typography READ typography WRITE setTypography NOTIFY changed)
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
    int lineSpacing() const { return spacing; }
    void setLineSpacing(int value)
    {
        change(spacing, std::clamp(value, 120, 200), "view/lineSpacing");
    }
    int paragraphGap() const { return paragraphSpace; }
    void setParagraphGap(int value)
    {
        change(paragraphSpace, std::clamp(value, 0, 100), "view/paragraphGap");
    }
    int focusScope() const { return scope; }
    void setFocusScope(int value) { change(scope, std::clamp(value, 0, 2), "view/focusScope"); }
    bool hideMarks() const { return marksHidden; }
    // The spelling dictionaries on this computer, such as "en_GB".
    Q_INVOKABLE static QStringList dictionaries();
    void setHideMarks(bool value) { change(marksHidden, value, "view/hideMarks"); }
    bool hintsSeen() const { return seen; }
    void setHintsSeen(bool value) { change(seen, value, "view/hintsSeen"); }
    bool buttonLabels() const { return labels; }
    void setButtonLabels(bool value) { change(labels, value, "view/buttonLabels"); }
    int sessionTarget() const { return target; }
    void setSessionTarget(int value);
    int sprintMinutes() const { return sprint; }
    bool spellCheck() const { return spelling; }
    int styleChecks() const { return style; }
    int typography() const { return smart; }
    void setTypography(int value) { change(smart, std::clamp(value, 0, 7), "edit/typography"); }
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
    bool labels = false;
    bool seen = false;
    bool marksHidden = true;
    int scope = 1;
    int spacing = 150;
    int paragraphSpace = 100;
    int target = 0;
    int sprint = 25;
    bool spelling = true;
    int style = 0;
    int smart = 7;
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
