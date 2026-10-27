#include "Backup.h"
#include "ProjectList.h"
#include "ProjectStore.h"
#include "Templates.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <private/qzipwriter_p.h>

namespace {

void writeFile(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(bytes);
}

QStringList titles(const ProjectList &list)
{
    QStringList out;
    for (int i = 0; i < list.rowCount(); ++i)
        out.append(list.data(list.index(i), ProjectList::TitleRole).toString());
    return out;
}

} // namespace

class TestProjectList : public QObject
{
    Q_OBJECT

private slots:
    void aBackupRestoresAsANewProject()
    {
        QTemporaryDir dir;
        QTemporaryDir backups;
        const QString folder = dir.filePath("ledger");
        QVERIFY(Templates::create(folder, "novel", "The Salt Ledger", "A").isEmpty());
        ProjectList list;
        list.setFolder(dir.path());
        QVERIFY(ProjectList::backups(folder, backups.path()).isEmpty());
        QString error;
        QVERIFY(!Backup::write(folder, backups.path(), 5, &error).isEmpty());
        const QVariantList found = ProjectList::backups(folder, backups.path());
        QCOMPARE(found.size(), 1);
        const QString zip = found.first().toMap().value("path").toString();
        QSignalSpy created(&list, &ProjectList::created);
        QCOMPARE(list.restoreBackup(zip), QString());
        QCOMPARE(created.size(), 1);
        // The old project stays, and the new one has a new title and folder.
        QCOMPARE(list.count(), 2);
        QVERIFY(titles(list).contains("The Salt Ledger (restored)"));
        QVERIFY(titles(list).contains("The Salt Ledger"));
        const QString restored = created.first().first().toString();
        QVERIFY(restored != folder);
        QVERIFY(QFile::exists(restored + "/binder.toml"));
        // A second restore makes a second folder.
        QCOMPARE(list.restoreBackup(zip), QString());
        QCOMPARE(list.count(), 3);
        QVERIFY(!list.restoreBackup(dir.filePath("none.zip")).isEmpty());
    }

    void aZipWithAPathOutsideTheFolderIsRefused()
    {
        QTemporaryDir dir;
        const QString zip = dir.filePath("evil/evil-2026-01-01T00-00-00-000.zip");
        QVERIFY(QDir().mkpath(dir.filePath("evil")));
        {
            QZipWriter writer(zip);
            writer.addFile("evil/project.toml", "x");
            writer.addFile("evil/../escaped.txt", "x");
            writer.close();
        }
        QString error;
        QVERIFY(!Backup::restore(zip, dir.filePath("target"), &error));
        QVERIFY(error.contains("unsafe"));
        QVERIFY(!QFile::exists(dir.filePath("escaped.txt")));
    }

    void renameChangesTheTitleTheFolderAndKeepsTheBackups()
    {
        QTemporaryDir dir;
        QTemporaryDir backups;
        const QString folder = dir.filePath("ledger");
        QVERIFY(Templates::create(folder, "novel", "The Salt Ledger", "A").isEmpty());
        ProjectList list;
        list.setFolder(dir.path());
        QVERIFY(!Backup::write(folder, backups.path(), 5).isEmpty());
        QVERIFY(!list.rename(folder, "   ", backups.path()).isEmpty());
        QCOMPARE(list.rename(folder, "The Tide Table", backups.path()), QString());
        QCOMPARE(titles(list), QStringList { "The Tide Table" });
        const QString renamed = dir.filePath("the-tide-table");
        QVERIFY(QFile::exists(renamed + "/project.toml"));
        QVERIFY(!QFileInfo::exists(folder));
        // The old zip is still the backup of the project.
        QVERIFY(Backup::latest(renamed, backups.path()).isValid());
        QVERIFY(!Backup::write(renamed, backups.path(), 5).isEmpty());
        QCOMPARE(Backup::list(renamed, backups.path()).size(), 2);
        // A new title with the same folder name keeps the folder.
        QCOMPARE(list.rename(renamed, "The Tide Table!", backups.path()), QString());
        QVERIFY(QFile::exists(renamed + "/project.toml"));
        QVERIFY(!list.rename(dir.filePath("nothing"), "X", backups.path()).isEmpty());
    }

    void aProjectOpenInAnotherWindowIsNotRenamedOrArchived()
    {
        QTemporaryDir dir;
        const QString folder = dir.filePath("ledger");
        QVERIFY(Templates::create(folder, "novel", "The Salt Ledger", "A").isEmpty());
        QVERIFY(QDir().mkpath(folder + "/.codicier"));
        QLockFile held(folder + "/.codicier/lock");
        held.setStaleLockTime(0);
        QVERIFY(held.tryLock(0));
        ProjectList list;
        list.setFolder(dir.path());
        QVERIFY(list.rename(folder, "Other", QString()).contains("another window"));
        QVERIFY(list.archive(folder).contains("another window"));
        QVERIFY(QFile::exists(folder + "/project.toml"));
        held.unlock();
        QCOMPARE(list.archive(folder), QString());
        QCOMPARE(list.count(), 0);
        QVERIFY(QFile::exists(dir.filePath("archive/ledger/project.toml")));
    }

    void duplicateCopiesTheProjectWithoutTheAppFolder()
    {
        QTemporaryDir dir;
        const QString folder = dir.filePath("ledger");
        QVERIFY(Templates::create(folder, "novel", "The Salt Ledger", "A").isEmpty());
        QVERIFY(QDir().mkpath(folder + "/.codicier"));
        writeFile(folder + "/.codicier/state.toml", "x = 1\n");
        ProjectList list;
        list.setFolder(dir.path());
        QCOMPARE(list.duplicate(folder), QString());
        QCOMPARE(list.count(), 2);
        QVERIFY(titles(list).contains("The Salt Ledger (copy)"));
        QVERIFY(QFile::exists(dir.filePath("the-salt-ledger-copy/project.toml")));
        QVERIFY(!QFileInfo::exists(dir.filePath("the-salt-ledger-copy/.codicier")));
        QVERIFY(!list.duplicate(dir.filePath("none")).isEmpty());
    }

    void listsTheProjectsNewestFirst()
    {
        QTemporaryDir dir;
        QVERIFY(
            Templates::create(dir.filePath("ledger"), "novel", "The Salt Ledger", "A").isEmpty());
        QThread::msleep(20);
        QVERIFY(Templates::create(dir.filePath("revision"), "essay", "On Revision", "A").isEmpty());
        QVERIFY(QDir(dir.path()).mkdir("not-a-project"));

        ProjectList list;
        list.setFolder(dir.path());
        QCOMPARE(list.count(), 2);
        QCOMPARE(titles(list), (QStringList { "On Revision", "The Salt Ledger" }));
        QCOMPARE(list.fictionCount(), 1);
        QCOMPARE(list.nonFictionCount(), 1);

        // A new edit moves a project to the top.
        QThread::msleep(20);
        writeFile(dir.filePath("ledger/manuscript/scene-1.md"),
                  "The lamp had burned down.\n\nShe found the letter.\n");
        list.refresh();
        QCOMPARE(titles(list), (QStringList { "The Salt Ledger", "On Revision" }));
        const QVariantMap first = list.get(0);
        QCOMPARE(first.value("words").toInt(), 9);
        QCOMPARE(list.totalWords(), 9);
        QCOMPARE(first.value("goal").toInt(), 80000);
        QCOMPARE(first.value("kindLabel").toString(), QStringLiteral("Novel"));
        QCOMPARE(first.value("fiction").toBool(), true);
        QCOMPARE(first.value("lastLine").toString(), QStringLiteral("She found the letter."));
        QCOMPARE(first.value("editedLabel").toString(), QStringLiteral("Today"));
        QCOMPARE(first.value("error").toString(), QString());
        QVERIFY(list.get(5).isEmpty());
    }

    void lastLineIsPlainProse()
    {
        QTemporaryDir dir;
        QVERIFY(Templates::create(dir.filePath("p"), "blank", "E", "").isEmpty());
        writeFile(
            dir.filePath("p/manuscript/untitled.md"),
            "# A heading\n\nThe letter said *the fourth* of **March**, _snake_case_ stays.[^1]\n\n"
            "[^1]: A footnote.\n");
        ProjectList list;
        list.setFolder(dir.path());
        QCOMPARE(list.get(0).value("lastLine").toString(),
                 QStringLiteral("The letter said the fourth of March, snake_case stays."));
    }

    void lastLineDropsLinksAndCitations()
    {
        QTemporaryDir dir;
        QVERIFY(Templates::create(dir.filePath("p"), "blank", "E", "").isEmpty());
        writeFile(dir.filePath("p/manuscript/untitled.md"),
                  "It was [[Elin]], ==not== [[Tomas|the brother]] [@kurlansky2002, p. 3].\n");
        ProjectList list;
        list.setFolder(dir.path());
        QCOMPARE(list.get(0).value("lastLine").toString(),
                 QStringLiteral("It was Elin, not Tomas."));
    }

    void brokenProjectStaysWithItsError()
    {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkdir("broken"));
        writeFile(dir.filePath("broken/project.toml"), "title = \"No end\n");
        ProjectList list;
        list.setFolder(dir.path());
        QCOMPARE(list.count(), 1);
        QCOMPARE(list.get(0).value("title").toString(), QStringLiteral("broken"));
        QVERIFY(list.get(0).value("error").toString().startsWith("project.toml:1:"));
        QCOMPARE(list.fictionCount(), 0);
    }

    void createMakesAProjectFolder()
    {
        QTemporaryDir dir;
        ProjectList list;
        list.setFolder(dir.path());
        QSignalSpy refreshed(&list, &ProjectList::refreshed);
        QSignalSpy created(&list, &ProjectList::created);

        QCOMPARE(list.create("  The Salt Ledger ", "novel", "A. Writer"), QString());
        QVERIFY(QFile::exists(dir.filePath("the-salt-ledger/project.toml")));
        QCOMPARE(list.count(), 1);
        QVERIFY(refreshed.size() >= 1);
        QCOMPARE(created.size(), 1);
        QCOMPARE(list.get(0).value("author").toString(), QString("A. Writer"));
        QCOMPARE(created.at(0).at(0).toString(), dir.filePath("the-salt-ledger"));

        // The same title again gets a new folder name.
        QCOMPARE(list.create("The Salt Ledger", "essay", QString()), QString());
        QVERIFY(QFile::exists(dir.filePath("the-salt-ledger-2/project.toml")));
        QCOMPARE(list.count(), 2);
    }

    void sampleLoadsAsAProject()
    {
        QTemporaryDir dir;
        ProjectList list;
        QCOMPARE(list.createSample(), QStringLiteral("Choose a workspace first."));
        list.setFolder(dir.path());
        QSignalSpy created(&list, &ProjectList::created);
        QCOMPARE(list.createSample(), QString());
        QCOMPARE(list.createSample(), QString());
        QCOMPARE(created.size(), 2);
        QCOMPARE(list.count(), 2);
        const QVariantMap sample = list.get(0);
        QCOMPARE(sample.value("error").toString(), QString());
        QVERIFY(sample.value("words").toInt() > 300);
        const LoadResult result = ProjectStore::load(dir.filePath("the-lighthouse-keeper"));
        QVERIFY2(result.ok(), qPrintable(result.ok() ? QString() : result.errors.first().text()));
        int entities = 0;
        for (const ProjectItem &item : result.project.items)
            entities += item.section == QLatin1String("codex") ? 1 : 0;
        QCOMPARE(entities, 3);
        QVERIFY(QFileInfo(dir.filePath("the-lighthouse-keeper-2/binder.toml")).isWritable());
    }

    void lastBackupNamesTheNewestZip()
    {
        QTemporaryDir work;
        QTemporaryDir backups;
        const QString project = work.filePath("lamp");
        QVERIFY(Templates::create(project, "novel", "Lamp", "").isEmpty());
        QCOMPARE(ProjectList::lastBackup(project, backups.path()), QString());
        QVERIFY(!Backup::write(project, backups.path(), 5).isEmpty());
        QVERIFY(ProjectList::lastBackup(project, backups.path()).startsWith("today at "));
        QCOMPARE(ProjectList::lastBackup(project, QString()), QString());
    }

    void templateOutlineFollowsTheBinder()
    {
        const QVariantList novel = ProjectList::templateOutline("novel");
        QCOMPARE(novel.size(), 6);
        QCOMPARE(novel.at(0).toMap().value("section").toString(), QString("front"));
        const QVariantMap scene = novel.at(4).toMap();
        QCOMPARE(scene.value("title").toString(), QString("Scene 1"));
        QCOMPARE(scene.value("depth").toInt(), 2);
        QVERIFY(ProjectList::templateOutline("no-such-kind").isEmpty());
    }

    void createRefusesBadInput()
    {
        QTemporaryDir dir;
        ProjectList list;
        QVERIFY(list.create("Title", "novel", QString()).contains("workspace"));
        list.setFolder(dir.path());
        QVERIFY(list.create("   ", "novel", QString()).contains("title"));
        QVERIFY(list.create("Poems", "poem", QString()).contains("not a kind"));
        QCOMPARE(list.count(), 0);
    }

    void newFolderInTheWorkspaceIsSeen()
    {
        QTemporaryDir dir;
        ProjectList list;
        list.setFolder(dir.path());
        QCOMPARE(list.count(), 0);
        QVERIFY(
            Templates::create(dir.filePath("outside"), "essay", "Made Elsewhere", "").isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(list.count(), 1, 5000);
    }

    void editedLabels_data()
    {
        QTest::addColumn<int>("daysAgo");
        QTest::addColumn<QString>("label");
        QTest::newRow("today") << 0 << "Today";
        QTest::newRow("yesterday") << 1 << "Yesterday";
        QTest::newRow("days") << 3 << "3 days ago";
        QTest::newRow("one week") << 8 << "1 week ago";
        QTest::newRow("weeks") << 20 << "2 weeks ago";
        QTest::newRow("month") << 60 << "August 2026";
    }

    void editedLabels()
    {
        QFETCH(int, daysAgo);
        QFETCH(QString, label);
        const QDateTime now(QDate(2026, 10, 18), QTime(12, 0));
        QCOMPARE(ProjectList::editedLabel(now.addDays(-daysAgo), now), label);
        QCOMPARE(ProjectList::editedLabel(QDateTime(), now), QString());
    }

    void kinds()
    {
        QCOMPARE(ProjectList::kindLabel("essay-collection"), QStringLiteral("Essay collection"));
        QVERIFY(ProjectList::isFiction("series"));
        QVERIFY(!ProjectList::isFiction("non-fiction"));
    }

    void roleNamesMatchTheData()
    {
        QTemporaryDir dir;
        QVERIFY(Templates::create(dir.filePath("p"), "novella", "N", "").isEmpty());
        ProjectList list;
        list.setFolder(dir.path());
        const QHash<int, QByteArray> roles = list.roleNames();
        QCOMPARE(roles.size(), 12);
        QCOMPARE(list.data(list.index(0), ProjectList::FolderRole).toString(), dir.filePath("p"));
        QVERIFY(list.data(list.index(0), ProjectList::EditedRole).toDateTime().isValid());
        QVERIFY(!list.data(list.index(0), Qt::DisplayRole).isValid());
        QVERIFY(!list.data(list.index(3), ProjectList::TitleRole).isValid());
        QCOMPARE(list.rowCount(list.index(0)), 0);
    }

    void removeDeletesTheFolderAndItsBackups()
    {
        QTemporaryDir dir;
        QTemporaryDir backups;
        ProjectList list;
        list.setFolder(dir.path());
        QCOMPARE(list.create("The Salt Ledger", "novel", QString()), QString());
        QCOMPARE(list.create("Other", "essay", QString()), QString());
        const QString folder = dir.filePath("the-salt-ledger");
        QVERIFY(!Backup::write(folder, backups.path(), 10).isEmpty());
        QVERIFY(!Backup::write(dir.filePath("other"), backups.path(), 10).isEmpty());

        QCOMPARE(list.remove(folder, backups.path()), QString());
        QVERIFY(!QFileInfo::exists(folder));
        QVERIFY(!QFileInfo::exists(QDir(backups.path()).filePath("the-salt-ledger")));
        QCOMPARE(list.count(), 1);
        // The other project and its backups stay.
        QVERIFY(QFileInfo::exists(dir.filePath("other/project.toml")));
        QCOMPARE(QDir(QDir(backups.path()).filePath("other")).entryList(QDir::Files).size(), 1);
    }

    void removeRefusesFoldersThatAreNotProjects()
    {
        QTemporaryDir dir;
        QTemporaryDir outside;
        ProjectList list;
        const QString notSet = list.remove(dir.path(), QString());
        QVERIFY(!notSet.isEmpty());
        list.setFolder(dir.path());
        QCOMPARE(list.create("Kept", "essay", QString()), QString());
        QCOMPARE(list.create("Kept", "essay", QString()), QString());

        // The workspace itself, a folder outside it, and a plain folder.
        QVERIFY(!list.remove(dir.path(), QString()).isEmpty());
        QVERIFY(QDir().mkpath(outside.filePath("elsewhere")));
        QFile toml(outside.filePath("elsewhere/project.toml"));
        QVERIFY(toml.open(QIODevice::WriteOnly));
        toml.close();
        QVERIFY(!list.remove(outside.filePath("elsewhere"), QString()).isEmpty());
        QVERIFY(QFileInfo::exists(outside.filePath("elsewhere/project.toml")));
        QVERIFY(QDir().mkpath(dir.filePath("plain")));
        QVERIFY(!list.remove(dir.filePath("plain"), QString()).isEmpty());
        QVERIFY(QFileInfo::exists(dir.filePath("plain")));

        // A link to a project: the link target must stay.
        QVERIFY(QFile::link(dir.filePath("kept"), dir.filePath("link")));
        QVERIFY(!list.remove(dir.filePath("link"), QString()).isEmpty());
        QVERIFY(QFileInfo::exists(dir.filePath("kept/project.toml")));
        QCOMPARE(list.count(), 2);
    }
};

QTEST_GUILESS_MAIN(TestProjectList)
#include "tst_projectlist.moc"
