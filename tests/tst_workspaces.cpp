#include "Workspaces.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class TestWorkspaces : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void init() { QFile::remove(Workspaces::settingsPath()); }

    void startsEmpty()
    {
        const Workspaces workspaces;
        QCOMPARE(workspaces.current(), QString());
        QVERIFY(workspaces.recent().isEmpty());
    }

    void openRemembersTheFolder()
    {
        QTemporaryDir dir;
        {
            Workspaces workspaces;
            QSignalSpy changed(&workspaces, &Workspaces::changed);
            QCOMPARE(workspaces.open(dir.path() + "/./"), QString());
            QCOMPARE(changed.size(), 1);
            QCOMPARE(workspaces.current(), QDir::cleanPath(dir.path()));
            QCOMPARE(workspaces.currentName(), QDir(dir.path()).dirName());
        }
        // A new instance, as after a restart, reads the settings file.
        const Workspaces again;
        QCOMPARE(again.current(), QDir::cleanPath(dir.path()));
        QCOMPARE(again.recent(), QStringList { QDir::cleanPath(dir.path()) });
    }

    void openUrlUsesTheLocalPath()
    {
        QTemporaryDir dir;
        Workspaces workspaces;
        QCOMPARE(workspaces.openUrl(QUrl::fromLocalFile(dir.path())), QString());
        QCOMPARE(workspaces.current(), QDir::cleanPath(dir.path()));
    }

    void badFoldersAreRefused()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("note.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();

        Workspaces workspaces;
        QSignalSpy changed(&workspaces, &Workspaces::changed);
        QVERIFY(workspaces.open(dir.filePath("missing")).contains("does not exist"));
        QVERIFY(workspaces.open(QString()).contains("does not exist"));
        QVERIFY(workspaces.open(dir.filePath("note.txt")).contains("not a folder"));
        QCOMPARE(changed.size(), 0);
        QCOMPARE(workspaces.current(), QString());
    }

    void recentListKeepsTheNewestTen()
    {
        QTemporaryDir root;
        Workspaces workspaces;
        QStringList folders;
        for (int i = 0; i < 12; ++i) {
            const QString folder = root.filePath(QString::number(i));
            QVERIFY(QDir().mkpath(folder));
            folders.append(folder);
            QCOMPARE(workspaces.open(folder), QString());
        }
        QCOMPARE(workspaces.recent().size(), Workspaces::recentLimit);
        QCOMPARE(workspaces.recent().first(), folders.at(11));
        QVERIFY(!workspaces.recent().contains(folders.at(0)));

        // Opening a folder again moves it to the front, with no copy.
        QCOMPARE(workspaces.open(folders.at(5)), QString());
        QCOMPARE(workspaces.recent().first(), folders.at(5));
        QCOMPARE(workspaces.recent().count(folders.at(5)), 1);
    }

    void forgetRemovesTheFolder()
    {
        QTemporaryDir a;
        QTemporaryDir b;
        Workspaces workspaces;
        QCOMPARE(workspaces.open(a.path()), QString());
        QCOMPARE(workspaces.open(b.path()), QString());

        workspaces.forget(a.path());
        QCOMPARE(workspaces.recent(), QStringList { b.path() });
        QCOMPARE(workspaces.current(), b.path());

        workspaces.forget(b.path());
        QCOMPARE(workspaces.current(), QString());
        QVERIFY(workspaces.recent().isEmpty());
        QVERIFY(QDir(b.path()).exists());

        QSignalSpy changed(&workspaces, &Workspaces::changed);
        workspaces.forget("/not/in/the/list");
        QCOMPARE(changed.size(), 0);
    }
};

QTEST_GUILESS_MAIN(TestWorkspaces)
#include "tst_workspaces.moc"
