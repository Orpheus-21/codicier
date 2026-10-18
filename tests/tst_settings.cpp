#include "Settings.h"
#include "Workspaces.h"

#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QUrl>

class TestSettings : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void init() { QFile::remove(Workspaces::settingsPath()); }

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
};

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
