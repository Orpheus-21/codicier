#include "Document.h"
#include "KeyMap.h"
#include "Reminder.h"
#include "Settings.h"
#include "Workspaces.h"

#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QUrl>

class TestSettings : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void init()
    {
        QFile::remove(Workspaces::settingsPath());
        QFile::remove(KeyMap().file());
    }

    void defaults()
    {
        const Settings settings;
        QCOMPARE(settings.backupFolder(), Settings::defaultBackupFolder());
        QVERIFY(settings.backupFolder().endsWith("/backups"));
        QCOMPARE(settings.backupsToKeep(), Settings::defaultBackupsToKeep);
    }

    void valuesSurviveARestart()
    {
        {
            Settings settings;
            QSignalSpy changed(&settings, &Settings::changed);
            settings.setBackupFolder("/tmp/somewhere/./backups/");
            settings.setBackupsToKeep(25);
            settings.setBackupsToKeep(25);
            QCOMPARE(changed.size(), 2);
        }
        const Settings again;
        QCOMPARE(again.backupFolder(), QStringLiteral("/tmp/somewhere/backups"));
        QCOMPARE(again.backupsToKeep(), 25);
    }

    void keepStaysInItsLimits()
    {
        Settings settings;
        settings.setBackupsToKeep(0);
        QCOMPARE(settings.backupsToKeep(), 1);
        settings.setBackupsToKeep(50000);
        QCOMPARE(settings.backupsToKeep(), 1000);
    }

    void folderFromAUrlAndNoFolder()
    {
        Settings settings;
        settings.setBackupFolderUrl(QUrl::fromLocalFile("/tmp/zips"));
        QCOMPARE(settings.backupFolder(), QStringLiteral("/tmp/zips"));
        settings.setBackupFolder(QString());
        QCOMPARE(settings.backupFolder(), QString());
    }

    void keyMapMovesAFormerDefault()
    {
        QFile::remove(KeyMap().file());
        const KeyMap keys;
        {
            QSettings file(keys.file(), QSettings::IniFormat);
            file.setValue("keys/inspector", "Ctrl+I");
            file.setValue("keys/binder", "Ctrl+Alt+X");
        }
        const QVariantList commands {
            QVariantMap { { "id", "italic" }, { "sequence", "Ctrl+I" } },
            QVariantMap {
                { "id", "inspector" }, { "sequence", "Ctrl+Shift+I" }, { "former", "Ctrl+I" } },
            QVariantMap {
                { "id", "binder" }, { "sequence", "Ctrl+Shift+B" }, { "former", "Ctrl+B" } },
        };
        const QVariantMap result = keys.resolve(commands);
        const QVariantMap sequences = result.value("sequences").toMap();
        QCOMPARE(sequences.value("italic").toString(), QStringLiteral("Ctrl+I"));
        QCOMPARE(sequences.value("inspector").toString(), QStringLiteral("Ctrl+Shift+I"));
        // The author chose this key, so it stays.
        QCOMPARE(sequences.value("binder").toString(), QStringLiteral("Ctrl+Alt+X"));
        QVERIFY(result.value("warnings").toStringList().isEmpty());
        QFile::remove(keys.file());
    }

    void oldDefaultKeyMovesToNoKey()
    {
        const KeyMap keys;
        {
            QSettings file(keys.file(), QSettings::IniFormat);
            file.setValue("keys/versions.list", "Ctrl+Alt+H");
            file.setValue("keys/timeline", "F9");
        }
        const QVariantList commands {
            QVariantMap {
                { "id", "versions.list" }, { "sequence", "" }, { "former", "Ctrl+Alt+H" } },
            QVariantMap { { "id", "timeline" }, { "sequence", "" }, { "former", "Ctrl+Alt+T" } },
        };
        const QVariantMap sequences = keys.resolve(commands).value("sequences").toMap();
        QCOMPARE(sequences.value("versions.list").toString(), QString());
        // A key that the author chose stays.
        QCOMPARE(sequences.value("timeline").toString(), QStringLiteral("F9"));
        QFile::remove(keys.file());
    }

    void keyMapWritesDefaultsAndReadsChanges()
    {
        const auto command = [](const char *id, const char *name, const char *sequence) {
            return QVariantMap { { "id", id }, { "name", name }, { "sequence", sequence } };
        };
        const QVariantList commands { command("scene.new", "New scene", "Ctrl+N"),
                                      command("chapter.new", "New chapter", "ctrl+shift+n"),
                                      command("focus", "Focus mode", "F11") };
        const KeyMap keys;
        QVERIFY(keys.file().endsWith("/keys.ini"));
        QVariantMap result = keys.resolve(commands);
        QVariantMap sequences = result.value("sequences").toMap();
        QCOMPARE(sequences.value("scene.new").toString(), QStringLiteral("Ctrl+N"));
        QCOMPARE(sequences.value("chapter.new").toString(), QStringLiteral("Ctrl+Shift+N"));
        QVERIFY(result.value("warnings").toStringList().isEmpty());

        // The file holds every command, so the author can change each key.
        {
            QSettings file(keys.file(), QSettings::IniFormat);
            QCOMPARE(file.value("keys/focus").toString(), QStringLiteral("F11"));
            file.setValue("keys/focus", "Ctrl+N");
            file.setValue("keys/chapter.new", "Ctrl+Nope");
            file.setValue("keys/scene.new", "");
        }
        result = keys.resolve(commands);
        sequences = result.value("sequences").toMap();
        // An empty key turns the shortcut off. A bad key gives a warning.
        QCOMPARE(sequences.value("scene.new").toString(), QString());
        QCOMPARE(sequences.value("chapter.new").toString(), QString());
        QCOMPARE(sequences.value("focus").toString(), QStringLiteral("Ctrl+N"));
        QCOMPARE(result.value("warnings").toStringList(),
                 QStringList { "\"Ctrl+Nope\" is not a valid key for New chapter." });

        // Two commands on one key: the first keeps it, and a warning shows.
        {
            QSettings file(keys.file(), QSettings::IniFormat);
            file.setValue("keys/scene.new", "Ctrl+N");
            file.setValue("keys/chapter.new", "Ctrl+Shift+N");
        }
        result = keys.resolve(commands);
        sequences = result.value("sequences").toMap();
        QCOMPARE(sequences.value("scene.new").toString(), QStringLiteral("Ctrl+N"));
        QCOMPARE(sequences.value("focus").toString(), QString());
        QCOMPARE(result.value("warnings").toStringList(),
                 QStringList {
                     "Ctrl+N is the key of New scene and Focus mode. Only New scene uses it." });
    }

    void reminderIsDueOnceADay()
    {
        const QDateTime morning(QDate(2026, 10, 22), QTime(9, 15));
        QVERIFY(Reminder::isDue("09:00", "", morning));
        QVERIFY(!Reminder::isDue("09:00", "2026-10-22", morning));
        QVERIFY(Reminder::isDue("09:00", "2026-10-21", morning));
        QVERIFY(!Reminder::isDue("10:00", "", morning));
        QVERIFY(!Reminder::isDue("", "", morning));
        // With no notification service, notify fails quietly, so the window
        // alerts in its place. The test points D-Bus at nothing, so no real
        // notification shows.
        qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent/codicier-test-bus");
        QVERIFY(!Reminder::notify("Time to write", "A test"));
        {
            Settings settings;
            settings.setReminderTime("25:00");
            QCOMPARE(settings.reminderTime(), QString());
            settings.setReminderTime("07:45");
            settings.setReminderShown("2026-10-22");
        }
        Settings again;
        QCOMPARE(again.reminderTime(), QStringLiteral("07:45"));
        QCOMPARE(again.reminderShown(), QStringLiteral("2026-10-22"));
        again.setReminderTime("");
        again.setReminderShown("");
    }

    void buttonLabelsSurviveARestart()
    {
        {
            Settings settings;
            QVERIFY(!settings.buttonLabels());
            settings.setButtonLabels(true);
        }
        Settings again;
        QVERIFY(again.buttonLabels());
        again.setButtonLabels(false);
    }

    void typographySurvivesARestartAndClamps()
    {
        {
            Settings settings;
            QCOMPARE(settings.typography(), 7);
            settings.setTypography(2);
        }
        Settings again;
        QCOMPARE(again.typography(), 2);
        again.setTypography(99);
        QCOMPARE(again.typography(), 7);
    }

    void spacingSurvivesARestartAndClamps()
    {
        {
            Settings settings;
            QCOMPARE(settings.lineSpacing(), 150);
            QCOMPARE(settings.paragraphGap(), 100);
            settings.setLineSpacing(180);
            settings.setParagraphGap(50);
        }
        Settings again;
        QCOMPARE(again.lineSpacing(), 180);
        QCOMPARE(again.paragraphGap(), 50);
        again.setLineSpacing(900);
        QCOMPARE(again.lineSpacing(), 200);
        again.setLineSpacing(150);
        again.setParagraphGap(100);
    }

    void focusScopeSurvivesARestartAndClamps()
    {
        {
            Settings settings;
            QCOMPARE(settings.focusScope(), 1);
            settings.setFocusScope(2);
        }
        Settings again;
        QCOMPARE(again.focusScope(), 2);
        again.setFocusScope(9);
        QCOMPARE(again.focusScope(), 3);
        again.setFocusScope(1);
    }

    void cardColourSurvivesARestart()
    {
        {
            Settings settings;
            QCOMPARE(settings.cardColour(), QStringLiteral("label"));
            settings.setCardColour("plaid");
            QCOMPARE(settings.cardColour(), QStringLiteral("label"));
            settings.setCardColour("pov");
        }
        Settings again;
        QCOMPARE(again.cardColour(), QStringLiteral("pov"));
        again.setCardColour("label");
        QVERIFY(!again.freeBoard());
        again.setFreeBoard(true);
        QVERIFY(Settings().freeBoard());
        again.setFreeBoard(false);
    }

    void hideMarksSurvivesARestart()
    {
        {
            Settings settings;
            QVERIFY(settings.hideMarks());
            settings.setHideMarks(false);
        }
        Settings again;
        QVERIFY(!again.hideMarks());
        again.setHideMarks(true);
    }

    void hintsSeenSurvivesARestart()
    {
        {
            Settings settings;
            QVERIFY(!settings.hintsSeen());
            settings.setHintsSeen(true);
        }
        Settings again;
        QVERIFY(again.hintsSeen());
        again.setHintsSeen(false);
    }

    void startSettingsSurviveARestart()
    {
        {
            Settings settings;
            QVERIFY(!settings.openLast());
            QVERIFY(settings.lastProject().isEmpty());
            settings.setOpenLast(true);
            settings.setLastProject(QStringLiteral("/tmp/harbour"));
        }
        Settings again;
        QVERIFY(again.openLast());
        QCOMPARE(again.lastProject(), QStringLiteral("/tmp/harbour"));
        again.setOpenLast(false);
        again.setLastProject(QString());
    }

    void viewSettingsSurviveARestartAndClamp()
    {
        {
            Settings settings;
            QCOMPARE(settings.theme(), QStringLiteral("system"));
            QCOMPARE(settings.readingSize(), 19);
            QCOMPARE(settings.measure(), 66);
            QCOMPARE(settings.autosaveDelay(), 1000);
            QSignalSpy changed(&settings, &Settings::changed);
            settings.setTheme("sepia");
            settings.setTheme("purple");
            settings.setReadingFont("DejaVu Serif");
            settings.setUiFont("DejaVu Sans");
            settings.setReadingSize(40);
            settings.setMeasure(10);
            settings.setAutosaveDelay(2000);
            settings.setReducedMotion(true);
            QCOMPARE(changed.size(), 7);
            QCOMPARE(Document::saveDelayMs, 2000);
        }
        Document::saveDelayMs = 1000;
        const Settings again;
        QCOMPARE(again.theme(), QStringLiteral("sepia"));
        QCOMPARE(again.readingFont(), QStringLiteral("DejaVu Serif"));
        QCOMPARE(again.uiFont(), QStringLiteral("DejaVu Sans"));
        QCOMPARE(again.readingSize(), 28);
        QCOMPARE(again.measure(), 45);
        QCOMPARE(again.autosaveDelay(), 2000);
        QVERIFY(again.reducedMotion());
        // The saved delay reaches the documents at the start.
        QCOMPARE(Document::saveDelayMs, 2000);

        // A theme name that the app does not know is the system theme.
        QSettings(Workspaces::settingsPath(), QSettings::IniFormat).setValue("view/theme", "neon");
        QCOMPARE(Settings().theme(), QStringLiteral("system"));
        Document::saveDelayMs = 1000;
    }

    void accentSurvivesAndRejectsUnknownNames()
    {
        Settings settings;
        QCOMPARE(settings.accent(), QStringLiteral("theme"));
        settings.setAccent(QStringLiteral("plum"));
        settings.setAccent(QStringLiteral("neon"));
        QCOMPARE(settings.accent(), QStringLiteral("plum"));
        QCOMPARE(Settings().accent(), QStringLiteral("plum"));
        QSettings(Workspaces::settingsPath(), QSettings::IniFormat).setValue("view/accent", "neon");
        QCOMPARE(Settings().accent(), QStringLiteral("theme"));
        QVERIFY(!settings.essayToolsEverywhere());
        settings.setEssayToolsEverywhere(true);
        QVERIFY(Settings().essayToolsEverywhere());
        QVERIFY(settings.calmFocus());
        settings.setCalmFocus(false);
        QVERIFY(!Settings().calmFocus());
        settings.setUiScale(200);
        QCOMPARE(Settings().uiScale(), 130);
        settings.setUiScale(50);
        QCOMPARE(settings.uiScale(), 100);
        settings.setTheme(QStringLiteral("contrast"));
        QCOMPARE(Settings().theme(), QStringLiteral("contrast"));
    }

    void sessionTargetSurvivesARestart()
    {
        {
            Settings settings;
            QCOMPARE(settings.sessionTarget(), 0);
            settings.setSessionTarget(500000);
            QCOMPARE(settings.sessionTarget(), 100000);
            settings.setSessionTarget(750);
            QCOMPARE(settings.sprintMinutes(), 25);
            settings.setSprintMinutes(0);
            QCOMPARE(settings.sprintMinutes(), 1);
            settings.setSprintMinutes(15);
            QCOMPARE(settings.styleChecks(), 0);
            settings.setStyleChecks(73);
            QCOMPARE(settings.styleChecks(), 9);
            QVERIFY(settings.versionOnClose());
            settings.setVersionOnClose(false);
        }
        QCOMPARE(Settings().sessionTarget(), 750);
        QCOMPARE(Settings().sprintMinutes(), 15);
        QCOMPARE(Settings().styleChecks(), 9);
        QVERIFY(!Settings().versionOnClose());
    }
};

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
