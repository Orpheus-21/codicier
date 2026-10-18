#include "ProjectStore.h"
#include "Watcher.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

namespace {

constexpr int waitMs = 5000;
// Long enough for a signal that must not come.
constexpr int quietMs = 600;
const char *scene = "manuscript/a-date-she-did-not-know.md";

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

// Writes in place, as a simple editor does.
void writeInPlace(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(bytes);
}

// Writes a new file and renames it over the old one, as Neovim and
// QSaveFile do.
void writeByRename(const QString &path, const QByteArray &bytes)
{
    QSaveFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(bytes);
    QVERIFY(file.commit());
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

class TestWatcher : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir dir;
    LoadResult loaded;

    QString path(const char *file) const { return dir.filePath(QString::fromUtf8(file)); }

private slots:
    void init()
    {
        QVERIFY(dir.isValid());
        QDir(dir.path()).removeRecursively();
        QDir().mkpath(dir.path());
        copyFixture(dir.path());
        loaded = ProjectStore::load(dir.path());
        QVERIFY(loaded.ok());
        // A new time stamp needs the clock to move on.
        QThread::msleep(20);
    }

    void outsideEditOfACleanFile()
    {
        Watcher watcher(loaded.project);
        QSignalSpy changed(&watcher, &Watcher::textChanged);
        writeInPlace(path(scene), "A new first line.\n");
        QTRY_COMPARE_WITH_TIMEOUT(changed.size(), 1, waitMs);
        QCOMPARE(changed.first().first().toString(), QString::fromUtf8(scene));
    }

    void saveByRenameIsSeenAgainAndAgain()
    {
        Watcher watcher(loaded.project);
        QSignalSpy changed(&watcher, &Watcher::textChanged);
        writeByRename(path(scene), "First rename.\n");
        QTRY_COMPARE_WITH_TIMEOUT(changed.size(), 1, waitMs);
        QThread::msleep(20);
        writeByRename(path(scene), "Second rename.\n");
        QTRY_COMPARE_WITH_TIMEOUT(changed.size(), 2, waitMs);
    }

    void ownSaveIsNotAnOutsideEdit()
    {
        Watcher watcher(loaded.project);
        QSignalSpy changed(&watcher, &Watcher::textChanged);
        writeByRename(path(scene), "The app saved this.\n");
        watcher.noteSaved(QString::fromUtf8(scene));
        QTest::qWait(quietMs);
        QCOMPARE(changed.size(), 0);
    }

    void touchWithNoChangeIsQuiet()
    {
        Watcher watcher(loaded.project);
        QSignalSpy changed(&watcher, &Watcher::textChanged);
        writeInPlace(path(scene), readFile(path(scene)));
        QTest::qWait(quietMs);
        QCOMPARE(changed.size(), 0);
    }

    void unsavedFileGivesOneConflictAndLosesNoText()
    {
        Watcher watcher(loaded.project);
        QSignalSpy conflicts(&watcher, &Watcher::conflict);
        QSignalSpy changed(&watcher, &Watcher::textChanged);
        const QString file = QString::fromUtf8(scene);
        watcher.setUnsaved(file, true);

        writeInPlace(path(scene), "Outside edit one.\n");
        QTRY_COMPARE_WITH_TIMEOUT(conflicts.size(), 1, waitMs);
        QCOMPARE(conflicts.first().at(0).toString(), file);
        QCOMPARE(conflicts.first().at(1).toByteArray(), QByteArray("Outside edit one.\n"));

        // More outside edits during the same conflict give no new prompt.
        QThread::msleep(20);
        writeInPlace(path(scene), "Outside edit two.\n");
        QTest::qWait(quietMs);
        QCOMPARE(conflicts.size(), 1);
        QCOMPARE(changed.size(), 0);
        // The watcher wrote nothing: the outside text is still on disk.
        QCOMPARE(readFile(path(scene)), QByteArray("Outside edit two.\n"));

        // After the author answers, a newer edit is a new conflict.
        watcher.resolve(file);
        QThread::msleep(20);
        writeInPlace(path(scene), "Outside edit three.\n");
        QTRY_COMPARE_WITH_TIMEOUT(conflicts.size(), 2, waitMs);
    }

    void binderEditUpdatesTheModel()
    {
        Watcher watcher(loaded.project);
        QSignalSpy changed(&watcher, &Watcher::projectChanged);
        QByteArray binder = readFile(path("binder.toml"));
        binder.replace("title = \"Part Two\"", "title = \"Part Two: Flood\"");
        writeByRename(path("binder.toml"), binder);
        QTRY_COMPARE_WITH_TIMEOUT(changed.size(), 1, waitMs);

        const LoadResult reloaded = ProjectStore::load(dir.path());
        QVERIFY(reloaded.ok());
        QCOMPARE(reloaded.project.items.at(5).title, QStringLiteral("Part Two: Flood"));
        watcher.track(reloaded.project);
    }

    void removedFileIsReported()
    {
        Watcher watcher(loaded.project);
        QSignalSpy removed(&watcher, &Watcher::fileRemoved);
        QVERIFY(QFile::remove(path(scene)));
        QTRY_COMPARE_WITH_TIMEOUT(removed.size(), 1, waitMs);
        QCOMPARE(removed.first().first().toString(), QString::fromUtf8(scene));
    }
};

QTEST_GUILESS_MAIN(TestWatcher)
#include "tst_watcher.moc"
