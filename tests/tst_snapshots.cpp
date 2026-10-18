#include "ProjectSession.h"
#include "Snapshots.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextCursor>
#include <QTextDocument>

namespace {

const char *scene = "manuscript/a-date-she-did-not-know.md";
const char *sceneId = "t6v4h9r2mz";

void copyFixture(const QString &dir)
{
    const QString source = QStringLiteral(FIXTURE_DIR "/novel");
    QDirIterator it(source, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString target = dir + path.mid(source.size());
        QDir().mkpath(QFileInfo(target).path());
        QVERIFY(QFile::copy(path, target));
        QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner);
    }
}

} // namespace

class TestSnapshots : public QObject
{
    Q_OBJECT

    QTemporaryDir dir;

    QByteArray read(const QString &file) const
    {
        QFile in(dir.filePath(file));
        return in.open(QIODevice::ReadOnly) ? in.readAll() : QByteArray();
    }

    void write(const QString &file, const QByteArray &bytes) const
    {
        QFile out(dir.filePath(file));
        QVERIFY(out.open(QIODevice::WriteOnly | QIODevice::Truncate));
        out.write(bytes);
    }

private slots:
    void init()
    {
        QDir(dir.path()).removeRecursively();
        QDir().mkpath(dir.path());
        copyFixture(dir.path());
    }

    void takeCopiesTheFile()
    {
        const QString path = Snapshots::take(dir.path(), sceneId, scene, "Before the rewrite");
        QVERIFY(path.startsWith(QStringLiteral("snapshots/t6v4h9r2mz/")));
        QVERIFY(path.endsWith(" Before the rewrite.md"));
        QCOMPARE(read(path), read(scene));
        const QList<Snapshots::Snapshot> list = Snapshots::list(dir.path(), sceneId);
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.first().title, QStringLiteral("Before the rewrite"));
        QVERIFY(list.first().time.isValid());
    }

    void titlesAreSafeFileNames()
    {
        const QString path = Snapshots::take(dir.path(), sceneId, scene, "a/b:c*\"d\". ");
        QVERIFY(path.endsWith(" a b c d.md"));
        QVERIFY(QFile::exists(dir.filePath(path)));
    }

    void snapshotsInOneSecondGetNewNames()
    {
        const QString first = Snapshots::take(dir.path(), sceneId, scene, QString());
        const QString second = Snapshots::take(dir.path(), sceneId, scene, QString());
        const QString third = Snapshots::take(dir.path(), sceneId, scene, "Third");
        QVERIFY(first != second && second != third && first != third);
        QCOMPARE(Snapshots::list(dir.path(), sceneId).size(), 3);
    }

    void listIsNewestFirstAndSkipsOtherFiles()
    {
        QDir().mkpath(dir.filePath("snapshots/t6v4h9r2mz"));
        write("snapshots/t6v4h9r2mz/2026-01-01T10-00-00 Old.md", "old\n");
        write("snapshots/t6v4h9r2mz/2026-03-01T10-00-00.md", "newer\n");
        write("snapshots/t6v4h9r2mz/notes.md", "not a snapshot\n");
        const QList<Snapshots::Snapshot> list = Snapshots::list(dir.path(), sceneId);
        QCOMPARE(list.size(), 2);
        QCOMPARE(list.at(0).title, QString());
        QCOMPARE(list.at(1).title, QStringLiteral("Old"));
        QVERIFY(Snapshots::list(dir.path(), "nothing").isEmpty());
    }

    void restoreKeepsTheCurrentText()
    {
        const QString old = Snapshots::take(dir.path(), sceneId, scene, "Old");
        write(scene, "The new text.\n");
        QVERIFY(Snapshots::restore(dir.path(), sceneId, scene, old));
        QVERIFY(read(scene).startsWith("The letter said"));
        // The text before the restore is a snapshot now.
        bool kept = false;
        for (const Snapshots::Snapshot &snapshot : Snapshots::list(dir.path(), sceneId))
            kept = kept
                || (snapshot.title == "Before restore" && read(snapshot.path) == "The new text.\n");
        QVERIFY(kept);
    }

    void restoreRefusesAnotherItemsSnapshot()
    {
        const QString other
            = Snapshots::take(dir.path(), "m1n2p3q4r5", "manuscript/the-river-office.md", "");
        const QByteArray before = read(scene);
        QVERIFY(!Snapshots::restore(dir.path(), sceneId, scene, other));
        QVERIFY(!Snapshots::restore(dir.path(), sceneId, scene,
                                    "snapshots/t6v4h9r2mz/../m1n2p3q4r5/x.md"));
        QVERIFY(!Snapshots::restore(dir.path(), sceneId, scene, "snapshots/t6v4h9r2mz/missing.md"));
        QCOMPARE(read(scene), before);
        QCOMPARE(Snapshots::take(dir.path(), sceneId, "manuscript/missing.md", ""), QString());
        QCOMPARE(Snapshots::take(dir.path(), QString(), scene, ""), QString());
    }

    void sessionSnapshotHoldsUnsavedTyping()
    {
        ProjectSession session;
        session.setFolder(dir.path());
        session.setCurrentId(sceneId);
        QTextDocument text;
        session.document()->attach(&text);
        QTextCursor cursor(&text);
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(" Typed.");
        QSignalSpy changed(&session, &ProjectSession::snapshotsChanged);
        const QString path = session.takeSnapshot(sceneId, "Typed");
        QVERIFY(read(path).endsWith("Typed.\n"));
        QCOMPARE(changed.size(), 1);
        QCOMPARE(session.snapshots(sceneId).size(), 1);
        QCOMPARE(session.snapshots(sceneId).first().toMap().value("title").toString(),
                 QStringLiteral("Typed"));
        QCOMPARE(session.takeSnapshot("f6g7h8j9k0", ""), QString());
    }

    void compareShowsTheWordChangesAndUnsavedTyping()
    {
        ProjectSession session;
        session.setFolder(dir.path());
        session.setCurrentId(sceneId);
        QTextDocument text;
        session.document()->attach(&text);
        const QString old = session.takeSnapshot(sceneId, "Old");
        text.setPlainText("The letter said the fifth of March, and nothing else.");
        QTextCursor cursor(&text);
        cursor.insertText("");
        const QVariantMap result = session.compare(sceneId, old);
        QVERIFY(result.value("addedWords").toInt() >= 1);
        QVERIFY(result.value("removedWords").toInt() >= 1);
        QString added;
        for (const QVariant &segment : result.value("segments").toList()) {
            if (segment.toMap().value("kind") == "added")
                added += segment.toMap().value("text").toString();
        }
        QVERIFY2(added.contains("fifth"), qPrintable(added));
        QVERIFY(session.compare(sceneId, "snapshots/other/x.md").isEmpty());
        QVERIFY(session.compare(sceneId, "snapshots/t6v4h9r2mz/missing.md").isEmpty());
        QVERIFY(session.compare("nothing", old).isEmpty());
    }

    void sessionBacksUpOnlyAfterAChange()
    {
        QTemporaryDir backups;
        QString zip;
        {
            ProjectSession session;
            session.setProperty("backupFolder", backups.path());
            session.setProperty("backupsToKeep", 5);
            session.setFolder(dir.path());
            // Opened and read, nothing changed: no backup.
            QCOMPARE(session.backup(), QString());
            QVERIFY(session.rename("t6v4h9r2mz", "Renamed"));
            zip = session.backup();
            QVERIFY(QFile::exists(zip));
            // Nothing changed since that backup.
            QCOMPARE(session.backup(), QString());
            session.takeSnapshot("t6v4h9r2mz", "");
        }
        // Closing the project made one more backup, after the snapshot.
        const QString name = QFileInfo(dir.path()).fileName();
        QCOMPARE(QDir(backups.filePath(name)).entryList({ "*.zip" }, QDir::Files).size(), 2);
    }

    void noBackupFolderMeansNoBackup()
    {
        ProjectSession session;
        session.setFolder(dir.path());
        QVERIFY(session.rename("t6v4h9r2mz", "Renamed"));
        QCOMPARE(session.backup(), QString());
    }

    void sessionRestoreReloadsTheEditor()
    {
        ProjectSession session;
        session.setFolder(dir.path());
        session.setCurrentId(sceneId);
        QTextDocument text;
        session.document()->attach(&text);
        const QString old = session.takeSnapshot(sceneId, "Old");
        QTextCursor cursor(&text);
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(" Unsaved then restored.");
        QVERIFY(session.restoreSnapshot(sceneId, old));
        QVERIFY(text.toPlainText().endsWith("The year is missing."));
        // The unsaved typing is in the "Before restore" snapshot.
        bool kept = false;
        for (const QVariant &entry : session.snapshots(sceneId)) {
            const QVariantMap map = entry.toMap();
            kept = kept
                || (map.value("title") == "Before restore"
                    && read(map.value("path").toString()).endsWith("Unsaved then restored.\n"));
        }
        QVERIFY(kept);
        QVERIFY(!session.restoreSnapshot("nothing", old));
    }
};

QTEST_MAIN(TestSnapshots)
#include "tst_snapshots.moc"
