#include "Versions.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

namespace {

// The output of git in a folder, for the checks.
QString git(const QString &folder, const QStringList &arguments)
{
    QProcess process;
    process.start("git", QStringList { "-C", folder } + arguments);
    process.waitForFinished();
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

void write(const QString &path, const QByteArray &bytes)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(bytes);
}

} // namespace

class TestVersions : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // The tests need the git program, as the feature does.
        QVERIFY2(Versions::available(), "git is not on this computer");
    }

    void turnOnMakesARepositoryAndAFirstVersion()
    {
        QTemporaryDir dir;
        write(dir.filePath("project.toml"), "title = \"A\"\n");
        write(dir.filePath(".codicier/.gitignore"), "*\n");
        write(dir.filePath(".codicier/index.db"), "data");
        Versions versions(dir.path());
        QVERIFY(!versions.isOn());
        QCOMPARE(versions.turnOn("A. Writer"), QString());
        QVERIFY(versions.isOn());
        QVERIFY(QFileInfo::exists(dir.filePath(".git")));
        QCOMPARE(git(dir.path(), { "log", "--format=%s" }), QStringLiteral("Versions on"));
        // The app folder stays out of the versions.
        QCOMPARE(git(dir.path(), { "ls-files" }), QStringLiteral("project.toml"));

        // No change saves no version. A change saves one.
        QCOMPARE(versions.save("Nothing", "A. Writer"), QString());
        QCOMPARE(git(dir.path(), { "rev-list", "--count", "HEAD" }), QStringLiteral("1"));
        write(dir.filePath("manuscript/one.md"), "Text.\n");
        QCOMPARE(versions.save("First scene", "A. Writer"), QString());
        QCOMPARE(git(dir.path(), { "log", "-1", "--format=%s" }), QStringLiteral("First scene"));
        // No signature, ever.
        QCOMPARE(git(dir.path(), { "log", "-1", "--format=%G?" }), QStringLiteral("N"));

        // The log of the project, newest first, and the log of one file.
        const QList<Versions::Entry> all = versions.log();
        QCOMPARE(all.size(), 2);
        QCOMPARE(all.first().message, QStringLiteral("First scene"));
        QCOMPARE(all.last().message, QStringLiteral("Versions on"));
        QCOMPARE(all.first().hash.size(), 40);
        QVERIFY(all.first().time.contains('T'));
        QCOMPARE(versions.log("manuscript/one.md").size(), 1);
        QCOMPARE(versions.log("project.toml").size(), 1);

        // A restore of the whole folder: the old files, and no new file.
        const QString first = all.last().hash;
        write(dir.filePath("project.toml"), "title = \"Changed\"\n");
        QCOMPARE(versions.save("Changed", "A. Writer"), QString());
        QCOMPARE(versions.restoreAll(first), QString());
        QFile toml(dir.filePath("project.toml"));
        QVERIFY(toml.open(QIODevice::ReadOnly));
        QCOMPARE(toml.readAll(), QByteArray("title = \"A\"\n"));
        QVERIFY(!QFileInfo::exists(dir.filePath("manuscript/one.md")));
        QVERIFY(!versions.restoreAll("--hard").isEmpty());
    }

    void aRepositoryAboveTheFolderIsUsed()
    {
        QTemporaryDir dir;
        git(dir.path(), { "init", "--quiet" });
        write(dir.filePath("other/notes.md"), "Not this project.\n");
        const QString project = dir.filePath("novel");
        write(project + "/project.toml", "title = \"B\"\n");
        Versions versions(project);
        QVERIFY(versions.isOn());
        QCOMPARE(versions.turnOn(QString()), QString());
        QVERIFY(!QFileInfo::exists(project + "/.git"));
        // Only the project folder goes into the version.
        QCOMPARE(git(dir.path(), { "ls-files" }), QStringLiteral("novel/project.toml"));
    }
};

QTEST_GUILESS_MAIN(TestVersions)
#include "tst_versions.moc"
