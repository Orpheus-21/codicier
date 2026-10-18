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
        for (const char *old : { "p-2026-01-01T10-00-00.zip", "p-2026-02-01T10-00-00.zip",
                                 "p-2026-03-01T10-00-00.zip", "other.zip" }) {
            QFile file(backups + "/p/" + old);
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        const QString zip = Backup::write(project, backups, 2);
        QVERIFY(!zip.isEmpty());
        const QStringList left = QDir(backups + "/p").entryList(QDir::Files, QDir::Name);
        QCOMPARE(left.size(), 3);
        QVERIFY(left.contains("other.zip"));
        QVERIFY(left.contains("p-2026-03-01T10-00-00.zip"));
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
};

QTEST_GUILESS_MAIN(TestBackup)
#include "tst_backup.moc"
