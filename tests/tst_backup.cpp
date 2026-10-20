#include "Backup.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <private/qzipreader_p.h>

namespace {

void copyFixture(const QString &dir)
{
    const QString source = QStringLiteral(FIXTURE_DIR "/novel");
    QDirIterator it(source, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString target = dir + path.mid(source.size());
        QDir().mkpath(QFileInfo(target).path());
        QVERIFY(QFile::copy(path, target));
    }
}

QByteArray read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

class TestBackup : public QObject
{
    Q_OBJECT

private slots:
    void zipHoldsTheProjectWithoutTheAppFolder()
    {
        QTemporaryDir root;
        const QString project = root.filePath("the-salt-ledger");
        QDir().mkpath(project);
        copyFixture(project);
        QDir().mkpath(project + "/.codicier");
        QFile index(project + "/.codicier/index.db");
        QVERIFY(index.open(QIODevice::WriteOnly));
        index.write("rebuildable");
        index.close();

        QString error;
        const QString zip = Backup::write(project, root.filePath("backups"), 10, &error);
        QVERIFY2(!zip.isEmpty(), qPrintable(error));
        QVERIFY(zip.startsWith(root.filePath("backups/the-salt-ledger/the-salt-ledger-")));

        QZipReader reader(zip);
        QVERIFY(reader.isReadable());
        QStringList names;
        for (const QZipReader::FileInfo &info : reader.fileInfoList()) {
            if (info.isFile)
                names.append(info.filePath);
        }
        QVERIFY(names.contains("the-salt-ledger/binder.toml"));
        QVERIFY(names.contains("the-salt-ledger/manuscript/a-date-she-did-not-know.md"));
        QVERIFY(!names.contains("the-salt-ledger/.codicier/index.db"));
        QCOMPARE(reader.fileData("the-salt-ledger/binder.toml"), read(project + "/binder.toml"));
    }

    void onlyTheNewestZipsStay()
    {
        QTemporaryDir root;
        const QString project = root.filePath("p");
        QDir().mkpath(project);
        copyFixture(project);
        const QString backups = root.filePath("backups");
        QDir().mkpath(backups + "/p");
        // Three real zips of the project, and a file that is not one.
        QStringList written;
        for (int i = 0; i < 3; ++i)
            written.append(QFileInfo(Backup::write(project, backups, 10)).fileName());
        QFile other(backups + "/p/other.zip");
        QVERIFY(other.open(QIODevice::WriteOnly));
        other.close();
        const QString zip = Backup::write(project, backups, 2);
        QVERIFY(!zip.isEmpty());
        const QStringList left = QDir(backups + "/p").entryList(QDir::Files, QDir::Name);
        QCOMPARE(left.size(), 3);
        QVERIFY(left.contains("other.zip"));
        QVERIFY(left.contains(written.last()));
        QVERIFY(left.contains(QFileInfo(zip).fileName()));
    }

    void badInputGivesAnError()
    {
        QTemporaryDir root;
        QString error;
        QCOMPARE(Backup::write(root.filePath("missing"), root.filePath("b"), 5, &error), QString());
        QCOMPARE(error, QStringLiteral("No project or no backup folder."));
        QCOMPARE(Backup::write(root.path(), QString(), 5), QString());
        QCOMPARE(Backup::write(root.path(), root.filePath("b"), 0), QString());
        // A file where the backup folder must be.
        QFile blocker(root.filePath("file"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        QCOMPARE(Backup::write(root.path(), root.filePath("file"), 5, &error), QString());
        QCOMPARE(error, QStringLiteral("Cannot make the backup folder."));
    }

    void removeAllDeletesOnlyThisProject()
    {
        // Two projects with the same folder name in two workspaces share a
        // backup folder. Only the created time tells them apart.
        QTemporaryDir root;
        const QString first = root.filePath("one/novel");
        const QString second = root.filePath("two/novel");
        copyFixture(first);
        copyFixture(second);
        const QString toml = QDir(second).filePath("project.toml");
        QByteArray text = read(toml);
        QVERIFY(text.contains("created = 2026-"));
        text.replace("created = 2026-", "created = 2025-");
        QFile out(toml);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(text);
        out.close();

        const QString backups = root.filePath("backups");
        for (int i = 0; i < 2; ++i) {
            QVERIFY(!Backup::write(first, backups, 10).isEmpty());
            QVERIFY(!Backup::write(second, backups, 10).isEmpty());
        }
        const QDir target(QDir(backups).filePath("novel"));
        QCOMPARE(target.entryList(QDir::Files).size(), 4);
        // The pruning of one project leaves the zips of the other.
        QVERIFY(!Backup::write(first, backups, 1).isEmpty());
        QCOMPARE(target.entryList(QDir::Files).size(), 3);
        QVERIFY(!Backup::write(first, backups, 2).isEmpty());
        QCOMPARE(target.entryList(QDir::Files).size(), 4);

        QString error;
        QCOMPARE(Backup::removeAll(first, backups, &error), 2);
        QCOMPARE(target.entryList(QDir::Files).size(), 2);
        QVERIFY(target.exists());
        QCOMPARE(Backup::removeAll(second, backups, &error), 2);
        // The empty folder goes too.
        QVERIFY(!target.exists());

        // No backups is no error.
        QCOMPARE(Backup::removeAll(first, backups, &error), 0);
        QCOMPARE(Backup::removeAll(first, QString(), &error), 0);

        // A project with no created time cannot be matched.
        QFile::remove(QDir(first).filePath("project.toml"));
        QCOMPARE(Backup::removeAll(first, backups, &error), -1);
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestBackup)
#include "tst_backup.moc"
