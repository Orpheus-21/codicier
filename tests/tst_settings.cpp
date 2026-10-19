#include "KeyMap.h"
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
};

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
