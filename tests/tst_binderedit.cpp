#include "NameGenerator.h"
#include "ProjectSession.h"
#include "ProjectStore.h"
#include "SpellChecker.h"
#include "Templates.h"
#include "Thesaurus.h"

#include <QDate>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextCursor>
#include <QTextDocument>
#include <QUrl>

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
        QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner);
    }
}

// The item with id after a fresh load from disk.
ProjectItem onDisk(const QString &folder, const QString &id, Project *project = nullptr)
{
    const LoadResult result = ProjectStore::load(folder);
    if (project != nullptr)
        *project = result.project;
    for (const ProjectItem &item : result.project.items) {
        if (item.id == id)
            return item;
    }
    return { };
}

int indexOf(const Project &project, const QString &id)
{
    for (qsizetype i = 0; i < project.items.size(); ++i) {
        if (project.items.at(i).id == id)
            return static_cast<int>(i);
    }
    return -1;
}

QString titleOf(const Project &project, int index)
{
    return index >= 0 ? project.items.at(index).title : QStringLiteral("(top)");
}

} // namespace

class TestBinderEdit : public QObject
{
    Q_OBJECT

    QTemporaryDir dir;
    std::unique_ptr<ProjectSession> session;

private slots:
    // The small dictionary of the fixtures, not the dictionaries of the system.
    void initTestCase()
    {
        SpellChecker::searchPaths() = { QStringLiteral(FIXTURE_DIR "/dict") };
        Thesaurus::searchPaths() = { QStringLiteral(FIXTURE_DIR "/thes") };
    }

    void init()
    {
        QDir(dir.path()).removeRecursively();
        QDir().mkpath(dir.path());
        copyFixture(dir.path());
        session = std::make_unique<ProjectSession>();
        session->setFolder(dir.path());
        QCOMPARE(session->error(), QString());
    }

    void addSceneIntoTheSelectedChapter()
    {
        session->setCurrentId("f6g7h8j9k0");
        const QString id = session->add("scene");
        QVERIFY(!id.isEmpty());
        QCOMPARE(session->currentId(), id);
        QCOMPARE(session->document()->file(), QStringLiteral("manuscript/new-scene.md"));

        Project project;
        const ProjectItem item = onDisk(dir.path(), id, &project);
        QCOMPARE(item.kind, QStringLiteral("scene"));
        QCOMPARE(item.title, QStringLiteral("New scene"));
        QCOMPARE(titleOf(project, item.parent), QStringLiteral("The Letter"));
        QVERIFY(!item.missingFile);
        // The new scene is the last in the chapter.
        QCOMPARE(project.items.at(item.parent).children.last(), indexOf(project, id));
    }

    void addSceneAfterTheSelectedScene()
    {
        session->setCurrentId("m1n2p3q4r5");
        const QString id = session->add("scene");
        Project project;
        const ProjectItem item = onDisk(dir.path(), id, &project);
        const QList<int> siblings = project.items.at(item.parent).children;
        QCOMPARE(project.items.at(siblings.at(0)).id, QStringLiteral("m1n2p3q4r5"));
        QCOMPARE(project.items.at(siblings.at(1)).id, id);
        QCOMPARE(project.items.at(siblings.at(2)).id, QStringLiteral("t6v4h9r2mz"));
    }

    void secondNewSceneGetsANewFile()
    {
        session->setCurrentId("f6g7h8j9k0");
        session->add("scene");
        session->setCurrentId("f6g7h8j9k0");
        session->add("scene");
        QCOMPARE(session->document()->file(), QStringLiteral("manuscript/new-scene-2.md"));
    }

    void addNoteAndPageGoToTheirSections()
    {
        session->setCurrentId("t6v4h9r2mz");
        const QString note = session->add("note");
        QCOMPARE(onDisk(dir.path(), note).section, QStringLiteral("notes"));
        QCOMPARE(onDisk(dir.path(), note).file, QStringLiteral("notes/new-note.md"));
        session->setCurrentId(QString());
        const QString page = session->add("page");
        QCOMPARE(onDisk(dir.path(), page).section, QStringLiteral("front"));
        // A page after a selected back matter page stays in the back matter.
        session->setCurrentId(page);
        const QString part = session->add("part");
        QCOMPARE(onDisk(dir.path(), part).section, QStringLiteral("manuscript"));
        QCOMPARE(onDisk(dir.path(), part).file, QString());
    }

    void addPartIntoAFolderOrAtTheTop()
    {
        session->setCurrentId("a1b2c3d4e5");
        const QString part = session->add("part");
        Project project;
        const ProjectItem item = onDisk(dir.path(), part, &project);
        QCOMPARE(item.parent, -1);
        QCOMPARE(project.roots.indexOf(indexOf(project, part)),
                 project.roots.indexOf(indexOf(project, "a1b2c3d4e5")) + 1);
    }

    void renameSavesTheTitle()
    {
        QSignalSpy edited(session.get(), &ProjectSession::edited);
        QVERIFY(session->rename("t6v4h9r2mz", "  The Fourth of March  "));
        QCOMPARE(onDisk(dir.path(), "t6v4h9r2mz").title, QStringLiteral("The Fourth of March"));
        QCOMPARE(edited.size(), 1);
        // The file name does not follow the title.
        QCOMPARE(onDisk(dir.path(), "t6v4h9r2mz").file,
                 QStringLiteral("manuscript/a-date-she-did-not-know.md"));
        QVERIFY(session->rename("t6v4h9r2mz", "The Fourth of March"));
        QCOMPARE(edited.size(), 1);
        QVERIFY(!session->rename("t6v4h9r2mz", "   "));
        QVERIFY(!session->rename("nothing", "Title"));
    }

    void trashKeepsTheFilesAndTheBranch()
    {
        session->setCurrentId("t6v4h9r2mz");
        QVERIFY(session->moveToTrash("f6g7h8j9k0"));
        QCOMPARE(session->currentId(), QString());
        Project project;
        const ProjectItem chapter = onDisk(dir.path(), "f6g7h8j9k0", &project);
        QCOMPARE(chapter.section, QStringLiteral("trash"));
        QCOMPARE(chapter.parent, -1);
        QCOMPARE(chapter.data["restore"].value_or(std::string()), std::string("a1b2c3d4e5"));
        QCOMPARE(chapter.children.size(), 2);
        QCOMPARE(onDisk(dir.path(), "t6v4h9r2mz").section, QStringLiteral("trash"));
        QVERIFY(QFile::exists(dir.filePath("manuscript/a-date-she-did-not-know.md")));
        QVERIFY(!session->moveToTrash("f6g7h8j9k0"));
    }

    void restoreReturnsToTheOldParent()
    {
        QVERIFY(session->moveToTrash("f6g7h8j9k0"));
        QVERIFY(session->restore("f6g7h8j9k0"));
        Project project;
        const ProjectItem chapter = onDisk(dir.path(), "f6g7h8j9k0", &project);
        QCOMPARE(titleOf(project, chapter.parent), QStringLiteral("Part One"));
        QCOMPARE(chapter.section, QStringLiteral("manuscript"));
        QVERIFY(!chapter.data.contains("restore"));
        QVERIFY(!session->restore("f6g7h8j9k0"));
    }

    void restoreWithoutParentGoesToTheTop()
    {
        // The fixture trash item points to a chapter; trash that chapter
        // first, so the old parent is in the trash too.
        QVERIFY(session->moveToTrash("f6g7h8j9k0"));
        QVERIFY(session->restore("e5f6g7h8j9"));
        Project project;
        const ProjectItem scene = onDisk(dir.path(), "e5f6g7h8j9", &project);
        QCOMPARE(scene.parent, -1);
        QCOMPARE(scene.section, QStringLiteral("manuscript"));
    }

    void ownSavesDoNotReloadTheProject()
    {
        QSignalSpy loaded(session.get(), &ProjectSession::loaded);
        QVERIFY(session->rename("t6v4h9r2mz", "Renamed"));
        QTest::qWait(600);
        QCOMPARE(loaded.size(), 0);
    }

    void moveBeforeASiblingIsInBinderToml()
    {
        QVERIFY(session->move("t6v4h9r2mz", "m1n2p3q4r5", "before"));
        Project project;
        const ProjectItem chapter = onDisk(dir.path(), "f6g7h8j9k0", &project);
        QCOMPARE(project.items.at(chapter.children.at(0)).id, QStringLiteral("t6v4h9r2mz"));
        QCOMPARE(project.items.at(chapter.children.at(1)).id, QStringLiteral("m1n2p3q4r5"));
    }

    void moveAfterTheNextSibling()
    {
        QVERIFY(session->move("m1n2p3q4r5", "t6v4h9r2mz", "after"));
        Project project;
        const ProjectItem chapter = onDisk(dir.path(), "f6g7h8j9k0", &project);
        QCOMPARE(project.items.at(chapter.children.at(1)).id, QStringLiteral("m1n2p3q4r5"));
    }

    void moveInsideAnotherPart()
    {
        QVERIFY(session->canMove("m1n2p3q4r5", "k3m9q2x7ab", "inside"));
        QVERIFY(session->move("m1n2p3q4r5", "k3m9q2x7ab", "inside"));
        Project project;
        const ProjectItem scene = onDisk(dir.path(), "m1n2p3q4r5", &project);
        QCOMPARE(titleOf(project, scene.parent), QStringLiteral("Part Two"));
        QCOMPARE(scene.section, QStringLiteral("manuscript"));
    }

    void moveANewPageAfterAnotherPage()
    {
        session->setCurrentId(QString());
        const QString page = session->add("page");
        QVERIFY(session->move(page, "d4f7k2m9pq", "after"));
        Project project;
        onDisk(dir.path(), page, &project);
        QStringList front;
        for (const int root : project.roots) {
            if (project.items.at(root).section == "front")
                front.append(project.items.at(root).id);
        }
        QCOMPARE(front, (QStringList { "d4f7k2m9pq", page }));
    }

    void invalidMovesChangeNothing_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<QString>("target");
        QTest::addColumn<QString>("position");
        QTest::newRow("into its own branch") << "a1b2c3d4e5" << "f6g7h8j9k0" << "inside";
        QTest::newRow("onto itself") << "a1b2c3d4e5" << "a1b2c3d4e5" << "after";
        QTest::newRow("part into a chapter") << "k3m9q2x7ab" << "f6g7h8j9k0" << "inside";
        QTest::newRow("into a scene") << "m1n2p3q4r5" << "t6v4h9r2mz" << "inside";
        QTest::newRow("scene beside a note") << "m1n2p3q4r5" << "s5t6v7w8x9" << "after";
        QTest::newRow("out of the trash") << "e5f6g7h8j9" << "t6v4h9r2mz" << "after";
        QTest::newRow("into the trash") << "m1n2p3q4r5" << "e5f6g7h8j9" << "after";
        QTest::newRow("unknown position") << "m1n2p3q4r5" << "t6v4h9r2mz" << "under";
        QTest::newRow("unknown id") << "nothing" << "t6v4h9r2mz" << "after";
    }

    void invalidMovesChangeNothing()
    {
        QFETCH(QString, id);
        QFETCH(QString, target);
        QFETCH(QString, position);
        QFile file(dir.filePath("binder.toml"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray before = file.readAll();
        file.close();
        QVERIFY(!session->canMove(id, target, position));
        QVERIFY(!session->move(id, target, position));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), before);
    }

    void moveByStepsAmongSiblings()
    {
        QVERIFY(session->moveBy("t6v4h9r2mz", -1));
        Project project;
        ProjectItem chapter = onDisk(dir.path(), "f6g7h8j9k0", &project);
        QCOMPARE(project.items.at(chapter.children.first()).id, QStringLiteral("t6v4h9r2mz"));
        QVERIFY(!session->moveBy("t6v4h9r2mz", -1));
        QVERIFY(session->moveBy("t6v4h9r2mz", 1));
        chapter = onDisk(dir.path(), "f6g7h8j9k0", &project);
        QCOMPARE(project.items.at(chapter.children.last()).id, QStringLiteral("t6v4h9r2mz"));
        QVERIFY(!session->moveBy("t6v4h9r2mz", 2));
        // Part One and Part Two swap, and the other sections do not count.
        QVERIFY(session->moveBy("k3m9q2x7ab", -1));
        onDisk(dir.path(), "k3m9q2x7ab", &project);
        QStringList manuscript;
        for (const int root : project.roots) {
            if (project.items.at(root).section == "manuscript")
                manuscript.append(project.items.at(root).title);
        }
        QCOMPARE(manuscript, (QStringList { "Part Two", "Part One" }));
        QVERIFY(!session->moveBy("k3m9q2x7ab", -1));
    }

    QByteArray readFile(const QString &file) const
    {
        QFile in(dir.filePath(file));
        return in.open(QIODevice::ReadOnly) ? in.readAll() : QByteArray();
    }

    void writeFile(const QString &file, const QByteArray &bytes) const
    {
        QFile out(dir.filePath(file));
        QVERIFY(out.open(QIODevice::WriteOnly | QIODevice::Truncate));
        out.write(bytes);
    }

    void splitLosesNoText()
    {
        const QString file = "manuscript/a-date-she-did-not-know.md";
        const QByteArray original
            = "First paragraph here.\n\nSecond paragraph here.\n\nThird one.\n";
        writeFile(file, original);
        session->reload();
        const QString text = QString::fromUtf8(original).chopped(1);
        const int cut = static_cast<int>(text.indexOf("Second"));

        const QString id = session->split("t6v4h9r2mz", cut - 1);
        QVERIFY2(!id.isEmpty(), qPrintable(session->lastError()));
        const ProjectItem added = onDisk(dir.path(), id);
        QCOMPARE(added.title, QStringLiteral("A Date She Did Not Know (2)"));
        QCOMPARE(added.data["pov"].value_or(std::string()), std::string("Mara"));
        QCOMPARE(added.file, QStringLiteral("manuscript/a-date-she-did-not-know-2.md"));
        QCOMPARE(session->currentId(), id);

        // The two parts and the whitespace at the cut are the old text.
        const QByteArray first = readFile(file);
        const QByteArray second = readFile(added.file);
        QCOMPARE(first, QByteArray("First paragraph here.\n"));
        QCOMPARE(second, QByteArray("Second paragraph here.\n\nThird one.\n"));
        QCOMPARE(first.chopped(1) + "\n\n" + second, original);

        // The new scene comes right after the old one.
        Project project;
        const ProjectItem chapter = onDisk(dir.path(), "f6g7h8j9k0", &project);
        QCOMPARE(project.items.at(chapter.children.at(1)).id, QStringLiteral("t6v4h9r2mz"));
        QCOMPARE(project.items.at(chapter.children.at(2)).id, id);

        // A merge gives back the old file.
        QVERIFY(session->mergeWithNext("t6v4h9r2mz"));
        QCOMPARE(readFile(file), original);
        QVERIFY(onDisk(dir.path(), id).section == "trash");
        QCOMPARE(readFile(added.file), second);
    }

    void splitSavesTheOpenTextFirst()
    {
        session->setCurrentId("t6v4h9r2mz");
        QTextDocument editor;
        session->document()->attach(&editor);
        QTextCursor cursor(&editor);
        cursor.movePosition(QTextCursor::End);
        cursor.insertText("\n\nTyped but not saved.");
        const int cut = static_cast<int>(editor.toPlainText().indexOf("Typed"));
        const QString id = session->split("t6v4h9r2mz", cut);
        QVERIFY(!id.isEmpty());
        QCOMPARE(readFile(onDisk(dir.path(), id).file), QByteArray("Typed but not saved.\n"));
        QVERIFY(
            readFile("manuscript/a-date-she-did-not-know.md").endsWith("The year is missing.\n"));
    }

    void splitRefusesAnEmptySide()
    {
        QCOMPARE(session->split("t6v4h9r2mz", 0), QString());
        QCOMPARE(session->lastError(),
                 QStringLiteral("There is no text on one side of the cursor."));
        QCOMPARE(session->split("t6v4h9r2mz", 100000), QString());
        QCOMPARE(session->split("f6g7h8j9k0", 3), QString());
        QCOMPARE(session->lastError(), QStringLiteral("Only a scene can split."));
    }

    void mergeJoinsNotesAndNeedsANextScene()
    {
        QVERIFY(session->mergeWithNext("m1n2p3q4r5"));
        const ProjectItem merged = onDisk(dir.path(), "m1n2p3q4r5");
        QVERIFY(QString::fromStdString(merged.data["notes"].value_or(std::string()))
                    .contains("count of days"));
        QVERIFY(readFile("manuscript/the-river-office.md").contains("The letter said"));
        QVERIFY(!session->mergeWithNext("m1n2p3q4r5"));
        QCOMPARE(session->nextScene("f6g7h8j9k0"), QString());
        QCOMPARE(session->nextScene("nothing"), QString());
    }

    void fieldsReadTheItem()
    {
        const QVariantMap fields = session->fields("t6v4h9r2mz");
        QCOMPARE(fields.value("title").toString(), QStringLiteral("A Date She Did Not Know"));
        QCOMPARE(fields.value("status").toString(), QStringLiteral("revised"));
        QCOMPARE(fields.value("pov").toString(), QStringLiteral("Mara"));
        QCOMPARE(fields.value("tags").toStringList(),
                 (QStringList { "letter", "mother", "accounts" }));
        QCOMPARE(fields.value("target").toInt(), 1500);
        QCOMPARE(fields.value("words").toInt(), 18);
        QVERIFY(fields.value("notes").toString().startsWith("The figure"));
        // A scene with no status is a draft. A part has no status.
        QCOMPARE(session->fields("m1n2p3q4r5").value("status").toString(), QStringLiteral("final"));
        QCOMPARE(session->fields("a1b2c3d4e5").value("status").toString(), QString());
        QVERIFY(session->fields("nothing").isEmpty());
    }

    void setFieldSavesToBinderToml()
    {
        QVERIFY(session->setField("t6v4h9r2mz", "pov", "  Tomas  "));
        QVERIFY(session->setField("t6v4h9r2mz", "status", "final"));
        QVERIFY(
            session->setField("t6v4h9r2mz", "tags", QStringList { " clue ", "clue", "", "river" }));
        QVERIFY(session->setField("t6v4h9r2mz", "target", 2000));
        QVERIFY(session->setField("t6v4h9r2mz", "synopsis", "A new line.\nAnd another."));
        const ProjectItem item = onDisk(dir.path(), "t6v4h9r2mz");
        QCOMPARE(item.data["pov"].value_or(std::string()), std::string("Tomas"));
        QCOMPARE(item.data["status"].value_or(std::string()), std::string("final"));
        QCOMPARE(item.data["tags"].as_array()->size(), std::size_t(2));
        QCOMPARE(item.data["target"].value_or(0), 2000);
        QCOMPARE(item.data["synopsis"].value_or(std::string()),
                 std::string("A new line.\nAnd another."));
        // An unknown key in the item stays.
        QCOMPARE(item.data["mood"].value_or(std::string()), std::string("quiet"));
    }

    void emptyValuesRemoveTheKey()
    {
        QVERIFY(session->setField("t6v4h9r2mz", "pov", "   "));
        QVERIFY(session->setField("t6v4h9r2mz", "tags", QStringList()));
        QVERIFY(session->setField("t6v4h9r2mz", "target", 0));
        const ProjectItem item = onDisk(dir.path(), "t6v4h9r2mz");
        QVERIFY(!item.data.contains("pov"));
        QVERIFY(!item.data.contains("tags"));
        QVERIFY(!item.data.contains("target"));
    }

    void badFieldsAreRefused()
    {
        QVERIFY(!session->setField("t6v4h9r2mz", "status", "done"));
        QVERIFY(!session->setField("t6v4h9r2mz", "target", -5));
        QVERIFY(!session->setField("t6v4h9r2mz", "target", "many"));
        QVERIFY(!session->setField("t6v4h9r2mz", "id", "zzzzzzzzzz"));
        QVERIFY(!session->setField("nothing", "pov", "Mara"));
        QCOMPARE(onDisk(dir.path(), "t6v4h9r2mz").data["status"].value_or(std::string()),
                 std::string("revised"));
    }

    void textsUnderAChapterOrAPart()
    {
        const QVariantList chapter = session->textsUnder("f6g7h8j9k0");
        QCOMPARE(chapter.size(), 2);
        QCOMPARE(chapter.at(0).toMap().value("id").toString(), QStringLiteral("m1n2p3q4r5"));
        QCOMPARE(chapter.at(1).toMap().value("file").toString(),
                 QStringLiteral("manuscript/a-date-she-did-not-know.md"));
        QCOMPARE(session->textsUnder("a1b2c3d4e5").size(), 2);
        QCOMPARE(session->textsUnder("k3m9q2x7ab").size(), 0);
        QCOMPARE(session->textsUnder("t6v4h9r2mz").size(), 0);
        QCOMPARE(session->textsUnder("nothing").size(), 0);
    }

    void aSelectedChapterCountsItself()
    {
        session->setCurrentId("f6g7h8j9k0");
        QCOMPARE(session->chapterWords(), 28);
    }

    void includeIsWrittenOnlyWhenFalse()
    {
        QCOMPARE(session->fields("t6v4h9r2mz").value("include").toBool(), true);
        QVERIFY(session->setField("t6v4h9r2mz", "include", false));
        QCOMPARE(onDisk(dir.path(), "t6v4h9r2mz").data["include"].value_or(true), false);
        QCOMPARE(session->fields("t6v4h9r2mz").value("include").toBool(), false);
        QCOMPARE(session->manuscriptWords(), 10);
        QVERIFY(session->setField("t6v4h9r2mz", "include", true));
        QVERIFY(!onDisk(dir.path(), "t6v4h9r2mz").data.contains("include"));
        QCOMPARE(session->manuscriptWords(), 28);
        QVERIFY(!session->setField("t6v4h9r2mz", "include", "no"));
    }

    void emptyTrashDeletesOnlyTheTrash()
    {
        QTemporaryDir backups;
        session->setProperty("backupFolder", backups.path());
        // The chapter with its two scenes joins the old scene in the trash.
        QVERIFY(session->moveToTrash("f6g7h8j9k0"));
        session->takeSnapshot("e5f6g7h8j9", "");
        QCOMPARE(session->trashCount(), 4);
        QCOMPARE(session->emptyTrash(), 4);
        QCOMPARE(session->trashCount(), 0);

        QVERIFY(!QFile::exists(dir.filePath("manuscript/old-opening.md")));
        QVERIFY(!QFile::exists(dir.filePath("manuscript/a-date-she-did-not-know.md")));
        QVERIFY(!QDir(dir.filePath("snapshots/e5f6g7h8j9")).exists());
        QVERIFY(QFile::exists(dir.filePath("notes/the-ledger.md")));

        // The rest of the tree loads and is correct.
        Project project;
        const ProjectItem part = onDisk(dir.path(), "a1b2c3d4e5", &project);
        QCOMPARE(project.items.size(), 5);
        QCOMPARE(part.children.size(), 0);
        QCOMPARE(onDisk(dir.path(), "s5t6v7w8x9").title, QStringLiteral("The ledger"));

        // The backup came first and holds the old scene.
        const QString name = QFileInfo(dir.path()).fileName();
        QCOMPARE(QDir(backups.filePath(name)).entryList({ "*.zip" }, QDir::Files).size(), 1);
        QCOMPARE(session->emptyTrash(), 0);
    }

    void failedBackupKeepsTheTrash()
    {
        QTemporaryDir root;
        QFile blocker(root.filePath("file"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        session->setProperty("backupFolder", root.filePath("file"));
        QCOMPARE(session->emptyTrash(), -1);
        QCOMPARE(session->lastError(), QStringLiteral("The backup failed, so the trash stays."));
        QVERIFY(QFile::exists(dir.filePath("manuscript/old-opening.md")));
        QCOMPARE(session->trashCount(), 1);
    }

    void emptyTrashWithNoBackupFolder()
    {
        session->setCurrentId("e5f6g7h8j9");
        QCOMPARE(session->emptyTrash(), 1);
        QCOMPARE(session->currentId(), QString());
    }

    void searchFollowsTheProject()
    {
        const auto ids = [this](const QString &query) {
            QStringList list;
            for (const QVariant &hit : session->search(query))
                list.append(hit.toMap().value("id").toString());
            return list;
        };
        const QVariantList hits = session->search("fourth");
        QCOMPARE(hits.size(), 1);
        const QVariantMap hit = hits.first().toMap();
        QCOMPARE(hit.value("id").toString(), QStringLiteral("t6v4h9r2mz"));
        QVERIFY2(hit.value("snippet").toString().contains(
                     "The letter said the <b>fourth</b> of March, and nothing else"),
                 qPrintable(hit.value("snippet").toString()));
        // A footnote mark is not part of a snippet.
        const QString missing
            = session->search("missing").first().toMap().value("snippet").toString();
        QVERIFY2(!missing.contains("[^") && missing.contains("The year is <b>missing</b>."),
                 qPrintable(missing));
        QCOMPARE(session->firstMatch("t6v4h9r2mz", "FOURTH"), 21);
        QCOMPARE(session->firstMatch("t6v4h9r2mz", "lantern"), -1);
        QCOMPARE(session->firstMatch("nothere", "fourth"), -1);

        // Typing that is not saved yet is found.
        session->setCurrentId("t6v4h9r2mz");
        QTextDocument editor;
        session->document()->attach(&editor);
        QTextCursor cursor(&editor);
        cursor.insertText("A <lantern> burned. ");
        QCOMPARE(ids("lantern"), QStringList { "t6v4h9r2mz" });
        QVERIFY(session->search("lantern").first().toMap().value("snippet").toString().contains(
            "&lt;<b>lantern</b>&gt;"));

        // A rename changes the title that the search reads.
        QVERIFY(session->rename("f6g7h8j9k0", "Quarantine"));
        QCOMPARE(ids("quarantine"), QStringList { "f6g7h8j9k0" });

        // An item in the trash is not found.
        QVERIFY(session->moveToTrash("t6v4h9r2mz"));
        QVERIFY(ids("lantern").isEmpty());

        // An outside edit is found after the watcher sees it.
        const QString file = onDisk(dir.path(), "m1n2p3q4r5").file;
        QFile out(QDir(dir.path()).filePath(file));
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write("A heron stood still.\n");
        out.close();
        QTRY_COMPARE(ids("heron"), QStringList { "m1n2p3q4r5" });
        QVERIFY(session->search("   ").isEmpty());
    }

    void regexSearchFindsPatterns()
    {
        const QVariantList hits = session->search("f[a-z]+th of", true);
        QCOMPARE(hits.size(), 1);
        const QVariantMap hit = hits.first().toMap();
        QCOMPARE(hit.value("id").toString(), QStringLiteral("t6v4h9r2mz"));
        QVERIFY2(hit.value("snippet").toString().startsWith(
                     "The letter said the <b>fourth of</b> March"),
                 qPrintable(hit.value("snippet").toString()));
        QVariantMap match = session->matchIn("t6v4h9r2mz", "F[a-z]+TH", true);
        QCOMPARE(match.value("start").toInt(), 21);
        QCOMPARE(match.value("length").toInt(), 6);
        match = session->matchIn("t6v4h9r2mz", "fourth march", false);
        QCOMPARE(match.value("start").toInt(), 21);
        QCOMPARE(match.value("length").toInt(), 6);
        QCOMPARE(session->matchIn("t6v4h9r2mz", "x+", true).value("start").toInt(), -1);
        QCOMPARE(session->matchIn("t6v4h9r2mz", "x+", true).value("length").toInt(), 0);

        // A title match with no text match has no snippet.
        bool titleOnly = false;
        for (const QVariant &each : session->search("^Map of", true)) {
            if (each.toMap().value("title") == QStringLiteral("Map of the river"))
                titleOnly = each.toMap().value("snippet").toString().isEmpty();
        }
        QVERIFY(titleOnly);

        // A long text gives a short snippet with dots at both ends.
        session->setCurrentId("m1n2p3q4r5");
        QTextDocument editor;
        session->document()->attach(&editor);
        QTextCursor cursor(&editor);
        cursor.insertText(QString("word ").repeated(40) + "needle "
                          + QString("word ").repeated(40));
        const QString snippet
            = session->search("needle", true).first().toMap().value("snippet").toString();
        QVERIFY2(snippet.startsWith("...") && snippet.endsWith("...") && snippet.size() < 200,
                 qPrintable(snippet));

        // A pattern that matches only empty text finds nothing.
        QVERIFY(session->search("x*", true).isEmpty());
        // A bad pattern gives an error and no results.
        QCOMPARE(session->patternError("(unclosed"),
                 QStringLiteral("Missing closing parenthesis."));
        QCOMPARE(session->patternError("ok+"), QString());
        QVERIFY(session->search("(unclosed", true).isEmpty());
        QVERIFY(session->search("", true).isEmpty());
    }

    void replaceShowsThenChangesTheManuscript()
    {
        // The open text has typing that is not saved yet.
        session->setCurrentId("m1n2p3q4r5");
        QTextDocument editor;
        session->document()->attach(&editor);
        QTextCursor cursor(&editor);
        cursor.insertText("The river ran. The rivers met. ");
        const QVariantList preview = session->replacePreview("river", "canal", false);
        QCOMPARE(preview.size(), 1);
        const QVariantMap scene = preview.first().toMap();
        QCOMPARE(scene.value("id").toString(), QStringLiteral("m1n2p3q4r5"));
        // "rivers" is another word, and "River" has another case.
        QCOMPARE(scene.value("count").toInt(), 2);
        const QStringList lines = scene.value("lines").toStringList();
        QCOMPARE(lines.size(), 2);
        QVERIFY2(lines.first().startsWith("The <s>river</s> <b>canal</b> ran."),
                 qPrintable(lines.first()));
        QCOMPARE(session->snapshots("m1n2p3q4r5").size(), 0);

        QCOMPARE(session->replaceAll("river", "canal", false), 2);
        const QString after = QString::fromUtf8(readFile(onDisk(dir.path(), "m1n2p3q4r5").file));
        QVERIFY2(after.startsWith("The canal ran. The rivers met. The canal office"),
                 qPrintable(after));
        QCOMPARE(editor.toPlainText(), after.trimmed());
        // One snapshot holds the text before the change.
        const QVariantList snapshots = session->snapshots("m1n2p3q4r5");
        QCOMPARE(snapshots.size(), 1);
        QCOMPARE(snapshots.first().toMap().value("title").toString(),
                 QStringLiteral("Before replace"));
        QFile snapshot(
            QDir(dir.path()).filePath(snapshots.first().toMap().value("path").toString()));
        QVERIFY(snapshot.open(QIODevice::ReadOnly));
        QVERIFY(snapshot.readAll().startsWith("The river ran. The rivers met. The river office"));
        QVERIFY(session->replacePreview("river", "canal", false).isEmpty());

        // A pattern with groups. Notes are not in the manuscript.
        QCOMPARE(session->replaceAll("(f)ourth", "\\1ifth (\\0)", true), 1);
        QVERIFY(readFile("manuscript/a-date-she-did-not-know.md")
                    .startsWith("The letter said *the fifth (fourth) of March*"));
        QCOMPARE(session->snapshots("t6v4h9r2mz").size(), 1);
        QCOMPARE(session->replaceAll("(unclosed", "x", true), 0);
        QCOMPARE(session->replaceAll("", "x", false), 0);
    }

    void savedSearchLivesInTheBinder()
    {
        QSignalSpy texts(session.get(), &ProjectSession::textsChanged);
        const QString id = session->saveSearch("riv(er|ers)", true);
        QVERIFY(!id.isEmpty());
        QVERIFY(session->saveSearch("   ", false).isEmpty());

        // The binder file keeps the query, and regex only when it is true.
        Project project;
        const ProjectItem item = onDisk(dir.path(), id, &project);
        QCOMPARE(item.kind, QStringLiteral("search"));
        QCOMPARE(item.section, QStringLiteral("searches"));
        QCOMPARE(item.title, QStringLiteral("riv(er|ers)"));
        QCOMPARE(item.data["regex"].value_or(false), true);
        const QString plain = session->saveSearch("lantern", false);
        QVERIFY(!onDisk(dir.path(), plain).data.contains("regex"));

        const QVariantMap fields = session->fields(id);
        QCOMPARE(fields.value("query").toString(), QStringLiteral("riv(er|ers)"));
        QCOMPARE(fields.value("regex").toBool(), true);
        QCOMPARE(session->fields(plain).value("regex").toBool(), false);

        // A saved search is not a result of a search.
        for (const QVariant &hit : session->search("river"))
            QVERIFY(hit.toMap().value("id").toString() != id);
        for (const QVariant &hit : session->search("lantern", true))
            QVERIFY(hit.toMap().value("id").toString() != plain);
        QVERIFY(session->search("lantern").isEmpty());

        // A save of a text tells the views that a text changed.
        session->setCurrentId("t6v4h9r2mz");
        QTextDocument editor;
        session->document()->attach(&editor);
        QTextCursor(&editor).insertText("A lantern. ");
        QVERIFY(session->document()->save());
        QVERIFY(texts.size() > 0);
    }

    void itemsListsTheBinderWithPaths()
    {
        QVERIFY(session->moveToTrash("y0z1a2b3c4"));
        QStringList paths;
        QStringList ids;
        for (const QVariant &each : session->items()) {
            const QVariantMap item = each.toMap();
            ids.append(item.value("id").toString());
            if (item.value("id") == QStringLiteral("t6v4h9r2mz")) {
                QCOMPARE(item.value("title").toString(), QStringLiteral("A Date She Did Not Know"));
                QCOMPARE(item.value("kind").toString(), QStringLiteral("scene"));
                paths.append(item.value("path").toString());
            }
        }
        QCOMPARE(paths, QStringList { "Part One / The Letter" });
        QVERIFY(ids.contains("a1b2c3d4e5"));
        // An item in the trash is not in the list.
        QVERIFY(!ids.contains("y0z1a2b3c4"));
    }

    void viewStateSurvivesAReopen()
    {
        QVERIFY(session->viewState().isEmpty());
        QVERIFY(session->saveViewState({ { "item", "t6v4h9r2mz" },
                                         { "binder", false },
                                         { "binder-width", 280 },
                                         { "inspector-width", 351.6 } }));
        // The folder stays out of git, also with no search index.
        QCOMPARE(readFile(".codicier/.gitignore"), QByteArray("*\n"));

        ProjectSession again;
        again.setFolder(dir.path());
        const QVariantMap state = again.viewState();
        QCOMPARE(state.value("item").toString(), QStringLiteral("t6v4h9r2mz"));
        QCOMPARE(state.value("binder").typeId(), QMetaType::Bool);
        QCOMPARE(state.value("binder").toBool(), false);
        QCOMPARE(state.value("binder-width").toInt(), 280);
        QCOMPARE(state.value("inspector-width").toInt(), 351);

        // A bad file is no state, and the app can still write a new one.
        QFile file(QDir(dir.path()).filePath(".codicier/state.toml"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("binder = [unclosed");
        file.close();
        QVERIFY(again.viewState().isEmpty());
        QVERIFY(again.saveViewState({ { "binder", true } }));
        QCOMPARE(again.viewState().value("binder").toBool(), true);
    }

    void entitiesLiveInTheCodex()
    {
        QStringList ids;
        for (const char *kind : { "character", "place", "item", "faction", "lore", "term" }) {
            const QString id = session->add(kind);
            QVERIFY2(!id.isEmpty(), kind);
            ids.append(id);
        }
        const ProjectItem character = onDisk(dir.path(), ids.first());
        QCOMPARE(character.kind, QStringLiteral("character"));
        QCOMPARE(character.section, QStringLiteral("codex"));
        QVERIFY(character.file.startsWith("codex/"));
        QVERIFY(QFile::exists(QDir(dir.path()).filePath(character.file)));

        QVERIFY(session->setField(ids.first(), "aliases",
                                  QStringList { " Mara ", "", "the clerk", "Mara" }));
        QCOMPARE(session->fields(ids.first()).value("aliases").toStringList(),
                 (QStringList { "Mara", "the clerk" }));
        QVERIFY(session->setField(ids.first(), "aliases", QStringList()));
        QVERIFY(session->fields(ids.first()).value("aliases").toStringList().isEmpty());
        QVERIFY(session->setField(ids.first(), "aliases", QStringList { "Mara" }));

        // The binder file comes back byte for byte after a load and a save.
        Project project;
        onDisk(dir.path(), ids.first(), &project);
        QCOMPARE(ProjectStore::binderFile(project), readFile("binder.toml"));
        QVERIFY(readFile("binder.toml").contains("aliases = [\"Mara\"]"));
    }

    void customEntityTypesAndFields()
    {
        QVERIFY(session->add("ship").isEmpty());
        QCOMPARE(session->saveEntityType(QString(), "Ships",
                                         { "Captain", " Home port ", "", "Captain" }),
                 QStringLiteral("ships"));
        QVERIFY(session->saveEntityType(QString(), "ships", { }).isEmpty());
        QVERIFY(session->saveEntityType(QString(), "Scene", { }).isEmpty());
        QCOMPARE(session->saveEntityType("character", "Ignored", { "Age" }),
                 QStringLiteral("character"));

        QVariantMap ships;
        for (const QVariant &type : session->entityTypes()) {
            if (type.toMap().value("kind") == QStringLiteral("ships"))
                ships = type.toMap();
            if (type.toMap().value("kind") == QStringLiteral("character")) {
                QCOMPARE(type.toMap().value("name").toString(), QStringLiteral("Characters"));
                QCOMPARE(type.toMap().value("fields").toStringList(), QStringList { "Age" });
            }
        }
        QCOMPARE(ships.value("name").toString(), QStringLiteral("Ships"));
        QCOMPARE(ships.value("fields").toStringList(), (QStringList { "Captain", "Home port" }));
        QVERIFY(ships.value("custom").toBool());

        const QString id = session->add("ships");
        QVERIFY(!id.isEmpty());
        QVERIFY(session->setEntityField(id, "Captain", " Ahab "));
        QVERIFY(session->setEntityField(id, "Home port", "Nantucket"));
        QVERIFY(session->setEntityField(id, "Home port", ""));
        QCOMPARE(session->fields(id).value("values").toMap(),
                 (QVariantMap { { "Captain", "Ahab" } }));

        // Both files load again with no error and come back byte for byte.
        const LoadResult result = ProjectStore::load(dir.path());
        QVERIFY2(result.ok(), result.ok() ? "" : qPrintable(result.errors.first().text()));
        QCOMPARE(ProjectStore::projectFile(result.project), readFile("project.toml"));
        QCOMPARE(ProjectStore::binderFile(result.project), readFile("binder.toml"));
        const ProjectItem ship = onDisk(dir.path(), id);
        QCOMPARE(ship.section, QStringLiteral("codex"));

        // A built-in type with no fields leaves project.toml.
        QCOMPARE(session->saveEntityType("character", QString(), { }), QStringLiteral("character"));
        QVERIFY(!readFile("project.toml").contains("kind = \"character\""));
    }

    void linksFindEntitiesByNameAndAlias()
    {
        const QString id = session->add("character");
        QVERIFY(session->rename(id, "Mara Vell"));
        QVERIFY(session->setField(id, "aliases", QStringList { "the clerk" }));
        QCOMPARE(session->entityByName(" mara vell "), id);
        QCOMPARE(session->entityByName("The Clerk"), id);
        QVERIFY(session->entityByName("Abel").isEmpty());
        QVERIFY(session->entityByName("").isEmpty());
        // A scene is not an entity, even with the same title.
        QVERIFY(session->entityByName("A Date She Did Not Know").isEmpty());

        const QString text = "Then [[the clerk]] left, and [[Nobody]] came.";
        QCOMPARE(session->entityAt(text, 7), id);
        QCOMPARE(session->entityAt(text, 5), id);
        QVERIFY(session->entityAt(text, 2).isEmpty());
        QVERIFY(session->entityAt(text, 32).isEmpty());

        // An entity in the trash is not a link target.
        QVERIFY(session->moveToTrash(id));
        QVERIFY(session->entityByName("Mara Vell").isEmpty());
    }

    void mentionsFindTheTextsThatNameAnEntity()
    {
        const QString id = session->add("character");
        QVERIFY(session->rename(id, "Mara Vell"));
        QVERIFY(session->setField(id, "aliases", QStringList { "the clerk", "Mara" }));
        QVERIFY(session->mentionsOf(id).isEmpty());

        // The open text has typing that is not saved yet.
        session->setCurrentId("t6v4h9r2mz");
        QTextDocument editor;
        session->document()->attach(&editor);
        QTextCursor(&editor).insertText("Mara Vell met THE CLERK. Marathon. [[Mara]] ");
        QFile other(QDir(dir.path()).filePath(onDisk(dir.path(), "m1n2p3q4r5").file));
        QVERIFY(other.open(QIODevice::Append));
        other.write("The clerk slept.\n");
        other.close();

        const QVariantList mentions = session->mentionsOf(id);
        QCOMPARE(mentions.size(), 2);
        // Binder order, and "Marathon" is not a mention.
        QCOMPARE(mentions.at(0).toMap().value("id").toString(), QStringLiteral("m1n2p3q4r5"));
        QCOMPARE(mentions.at(0).toMap().value("count").toInt(), 1);
        QCOMPARE(mentions.at(1).toMap().value("id").toString(), QStringLiteral("t6v4h9r2mz"));
        QCOMPARE(mentions.at(1).toMap().value("count").toInt(), 3);
        // Only an entity has mentions.
        QVERIFY(session->mentionsOf("t6v4h9r2mz").isEmpty());
    }

    void childrenOfGivesTheCards()
    {
        const QVariantList cards = session->childrenOf("f6g7h8j9k0");
        QCOMPARE(cards.size(), 2);
        const QVariantMap first = cards.first().toMap();
        QCOMPARE(first.value("id").toString(), QStringLiteral("m1n2p3q4r5"));
        QCOMPARE(first.value("kind").toString(), QStringLiteral("scene"));
        QCOMPARE(first.value("words").toInt(), 10);
        QCOMPARE(cards.at(1).toMap().value("synopsis").toString(),
                 QStringLiteral("Mara finds a date in her mother's letter."));
        QVERIFY(session->childrenOf("nothere").isEmpty());
        QVERIFY(session->childrenOf("m1n2p3q4r5").isEmpty());
    }

    void outlineOfGivesTheRows()
    {
        const QVariantList rows = session->outlineOf("a1b2c3d4e5");
        QCOMPARE(rows.size(), 2);
        const QVariantMap second = rows.at(1).toMap();
        QCOMPARE(second.value("id").toString(), QStringLiteral("t6v4h9r2mz"));
        QCOMPARE(second.value("status").toString(), QStringLiteral("revised"));
        QCOMPARE(second.value("pov").toString(), QStringLiteral("Mara"));
        QCOMPARE(second.value("place").toString(), QStringLiteral("The river office"));
        QCOMPARE(second.value("words").toInt(), 18);
        QVERIFY(session->outlineOf("nothere").isEmpty());

        // An entity with custom fields gives its values too.
        QCOMPARE(session->saveEntityType(QString(), "Ships", { "Captain" }),
                 QStringLiteral("ships"));
        const QString ship = session->add("ships");
        QVERIFY(session->setEntityField(ship, "Captain", "Ahab"));
        const QVariantList own = session->outlineOf(ship);
        QCOMPARE(own.size(), 1);
        QCOMPARE(own.first().toMap().value("fields").toMap().value("Captain").toString(),
                 QStringLiteral("Ahab"));
    }

    void sessionCountsTheAddedWords()
    {
        QCOMPARE(session->sessionWords(), 0);
        session->setCurrentId("t6v4h9r2mz");
        QTextDocument editor;
        session->document()->attach(&editor);
        QTextCursor(&editor).insertText("Four new words here. ");
        QTRY_COMPARE(session->sessionWords(), 4);
        // Fewer words than at the start count as 0.
        editor.setPlainText("Short.");
        QTRY_COMPARE(session->sessionWords(), 0);
    }

    void goalAndDeadlineGiveTheDailyWords()
    {
        // The fixture has a goal of 90000 and a deadline of 2027-03-01.
        QCOMPARE(session->goalWords(), 90000);
        QCOMPARE(session->deadline(), QStringLiteral("2027-03-01"));
        const QString date = QDate::currentDate().addDays(9).toString(Qt::ISODate);
        QVERIFY(session->setGoal(1028, date));
        QCOMPARE(session->deadline(), date);
        // 1028 goal minus 28 words is 1000, over 10 days with today.
        QCOMPARE(session->dailyNeeded(), 100);
        QVERIFY(readFile("project.toml").contains("deadline = " + date.toUtf8()));

        QVERIFY(!session->setGoal(1000, "next week"));
        QVERIFY(!session->setGoal(-1, ""));
        QVERIFY(session->setGoal(1000, QDate::currentDate().addDays(-1).toString(Qt::ISODate)));
        QCOMPARE(session->dailyNeeded(), -1);
        QVERIFY(session->setGoal(0, ""));
        QCOMPARE(session->goalWords(), 0);
        QVERIFY(!readFile("project.toml").contains("deadline ="));
        QVERIFY(!readFile("project.toml").contains("words = "));
    }

    void historyCountsTheWordsOfToday()
    {
        QCOMPARE(session->streak(), 0);
        session->setCurrentId("t6v4h9r2mz");
        QTextDocument editor;
        session->document()->attach(&editor);
        QTextCursor(&editor).insertText("Five more words go here. ");
        QTRY_COMPARE(session->sessionWords(), 5);
        QCOMPARE(session->streak(), 1);
        const QVariantList days = session->historyDays(3);
        QCOMPARE(days.size(), 3);
        QCOMPARE(days.last().toMap().value("words").toInt(), 5);
        QCOMPARE(days.last().toMap().value("date").toString(),
                 QDate::currentDate().toString(Qt::ISODate));
        // The save of the text writes the history too.
        QVERIFY(session->document()->save());
        QVERIFY(readFile("history.toml").contains(" = 5\n"));
    }

    void thesaurusGivesSynonyms()
    {
        QCOMPARE(session->synonyms("Lamp"), (QStringList { "light", "lantern", "beacon" }));
        QCOMPARE(session->synonyms("sea"), (QStringList { "ocean", "deep" }));
        QVERIFY(session->synonyms("ledger").isEmpty());
        // A thesaurus with no .idx file gives the same.
        Thesaurus::searchPaths() = { QStringLiteral(FIXTURE_DIR "/thes-noidx") };
        const Thesaurus plain(QStringLiteral("en_TEST"));
        QVERIFY(plain.isLoaded());
        QCOMPARE(plain.synonyms("sea"), (QStringList { "ocean", "deep" }));
        Thesaurus::searchPaths() = { QStringLiteral(FIXTURE_DIR "/thes") };
    }

    void languageChoosesTheDictionary()
    {
        QCOMPARE(ProjectSession::dictionaries(), (QStringList { "en_TEST", "fr_TEST" }));
        QVERIFY(!session->setLanguage("xx_XX"));
        QVERIFY(session->setLanguage("fr_TEST"));
        QCOMPARE(session->dictionary(), QStringLiteral("fr_TEST"));
        QVERIFY(readFile("project.toml").contains("language = \"fr-TEST\""));
        QVERIFY(session->isWordCorrect("lettre"));
        QVERIFY(!session->isWordCorrect("letter"));
        // No French thesaurus is in the test folder.
        QVERIFY(session->synonyms("lamp").isEmpty());
    }

    void plainNamesFindTheirEntity()
    {
        const QString id = session->add("character");
        QVERIFY(session->rename(id, "Mara Vell"));
        QVERIFY(session->setField(id, "aliases", QStringList { "the clerk" }));
        const QString text = QStringLiteral("Then Mara Vell met the clerk.");
        QCOMPARE(session->entityAt(text, 7), id);
        QCOMPARE(session->entityAt(text, 24), id);
        QVERIFY(session->entityAt(text, 1).isEmpty());
        QVERIFY(session->summaryOf("no-such-item").isEmpty());
        // The summary is the first paragraph of the text of the entity. Another
        // item opens first, so the file holds the text.
        session->setCurrentId("m1n2p3q4r5");
        const QString file = session->fields(id).value("file").toString();
        QFile out(QDir(session->folder()).filePath(file));
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write("A clerk of the river office.\nShe keeps the ledgers.\n\nBorn in Varn.\n");
        out.close();
        QCOMPARE(session->summaryOf(id),
                 QStringLiteral("A clerk of the river office. She keeps the ledgers."));
    }

    void projectDictionaryAndCodexNames()
    {
        QCOMPARE(session->dictionary(), QStringLiteral("en_TEST"));
        QVERIFY(session->isWordCorrect("letter"));
        QVERIFY(!session->isWordCorrect("Zork"));
        // A name of the codex is a word of the story.
        const QString id = session->add("character");
        QVERIFY(session->rename(id, "Zork Vale"));
        QVERIFY(session->isWordCorrect("Zork"));
        QVERIFY(session->setField(id, "aliases", QStringList { "the Quellmar" }));
        QVERIFY(session->isWordCorrect("Quellmar"));

        QVERIFY(!session->isWordCorrect("Brindle"));
        QVERIFY(session->addToDictionary(" Brindle "));
        QVERIFY(session->addToDictionary("Aldous"));
        QVERIFY(session->addToDictionary("Brindle"));
        QVERIFY(!session->addToDictionary("  "));
        QVERIFY(session->isWordCorrect("Brindle"));
        QCOMPARE(readFile("dictionary.txt"), QByteArray("Aldous\nBrindle\n"));

        // A new session reads the word list.
        ProjectSession again;
        again.setFolder(dir.path());
        QVERIFY(again.isWordCorrect("Brindle"));
        QVERIFY(again.isWordCorrect("Zork"));
    }

    void saveVersionCommitsTheLatestTyping()
    {
        QCOMPARE(session->saveVersion("Too soon"), QStringLiteral("Turn on versions first."));
        QCOMPARE(session->turnOnVersions(), QString());
        QVERIFY(session->versionsOn());
        session->setCurrentId("t6v4h9r2mz");
        QTextDocument editor;
        session->document()->attach(&editor);
        QTextCursor(&editor).insertText("New words. ");
        QCOMPARE(session->saveVersion(" Draft two "), QString());
        QProcess git;
        git.start("git", { "-C", dir.path(), "log", "--format=%s" });
        git.waitForFinished();
        const QStringList subjects
            = QString::fromUtf8(git.readAllStandardOutput()).trimmed().split('\n');
        QCOMPARE(subjects.first(), QStringLiteral("Draft two"));
        QCOMPARE(subjects.last(), QStringLiteral("Versions on"));
        QTextCursor(&editor).insertText("More. ");
        QCOMPARE(session->saveVersion(QString()), QString());
        git.start("git", { "-C", dir.path(), "log", "-1", "--format=%s" });
        git.waitForFinished();
        QVERIFY(QString::fromUtf8(git.readAllStandardOutput()).startsWith("Version of "));
    }

    void compareWithASavedVersion()
    {
        QCOMPARE(session->turnOnVersions(), QString());
        const QString hash
            = session->versions("t6v4h9r2mz").first().toMap().value("hash").toString();
        session->setCurrentId("t6v4h9r2mz");
        QTextDocument editor;
        session->document()->attach(&editor);
        QTextCursor cursor(&editor);
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(" Then rain came.");
        const QVariantMap result = session->compare("t6v4h9r2mz", "version:" + hash);
        QCOMPARE(result.value("addedWords").toInt(), 3);
        QCOMPARE(result.value("removedWords").toInt(), 0);
        // A bad hash or a file that the version does not hold gives nothing.
        QVERIFY(session->compare("t6v4h9r2mz", "version:--help").isEmpty());
        QVERIFY(session->compare("t6v4h9r2mz", "version:0000000").isEmpty());
    }

    void restoreATextAndTheProject()
    {
        QCOMPARE(session->turnOnVersions(), QString());
        const QString first = session->versions().first().toMap().value("hash").toString();
        const QByteArray before = readFile("manuscript/a-date-she-did-not-know.md");

        // A text: the old words come back, and the state before is a version.
        session->setCurrentId("t6v4h9r2mz");
        QTextDocument editor;
        session->document()->attach(&editor);
        editor.setPlainText("All new words.");
        QCOMPARE(session->restoreVersion("t6v4h9r2mz", first), QString());
        QCOMPARE(readFile("manuscript/a-date-she-did-not-know.md"), before);
        QVERIFY(editor.toPlainText().startsWith("The letter said"));
        QCOMPARE(session->versions().first().toMap().value("message").toString(),
                 QStringLiteral("Before the restore of A Date She Did Not Know"));
        QVERIFY(!session->restoreVersion("t6v4h9r2mz", "0000000").isEmpty());

        // The project: a scene added after the version goes away.
        const QString added = session->add("scene");
        QVERIFY(!added.isEmpty());
        QCOMPARE(session->restoreProject(first), QString());
        QVERIFY(onDisk(dir.path(), added).id.isEmpty());
        QCOMPARE(session->versions().first().toMap().value("message").toString(),
                 QStringLiteral("Restored a version"));
        QCOMPARE(session->versions().at(1).toMap().value("message").toString(),
                 QStringLiteral("Before the restore of the project"));
    }

    void sessionListsTheFrameworks()
    {
        const QVariantList frameworks = session->frameworks();
        QCOMPARE(frameworks.size(), 5);
        const QVariantMap cat = frameworks.at(1).toMap();
        QCOMPARE(cat.value("name").toString(), QStringLiteral("Save the Cat"));
        const QVariantMap catalyst = cat.value("beats").toList().at(3).toMap();
        QCOMPARE(catalyst.value("id").toString(), QStringLiteral("catalyst"));
        QCOMPARE(catalyst.value("at").toDouble(), 0.1);
        QVERIFY(!catalyst.value("description").toString().isEmpty());
    }

    void duplicateCopiesTextsAndFields()
    {
        // The chapter f6g7h8j9k0 holds two scenes with texts and fields.
        const QString copy = session->duplicate("f6g7h8j9k0");
        QVERIFY(!copy.isEmpty());
        QCOMPARE(session->fields(copy).value("title").toString(),
                 QStringLiteral("The Letter (copy)"));
        const QVariantList scenes = session->childrenOf(copy);
        QCOMPARE(scenes.size(), 2);
        const QString scene = scenes.at(1).toMap().value("id").toString();
        QVERIFY(scene != "t6v4h9r2mz");
        QCOMPARE(session->fields(scene).value("synopsis").toString(),
                 session->fields("t6v4h9r2mz").value("synopsis").toString());
        QCOMPARE(session->fields(scene).value("title").toString(),
                 QStringLiteral("A Date She Did Not Know"));
        const QString file = session->fields(scene).value("file").toString();
        QVERIFY(file != session->fields("t6v4h9r2mz").value("file").toString());
        QVERIFY(
            readFile(file).startsWith(readFile("manuscript/a-date-she-did-not-know.md").left(20)));
        // The copy sits right after the original.
        QCOMPARE(session->binder()->rowOf(copy) > session->binder()->rowOf("t6v4h9r2mz"), true);
        QVERIFY(session->duplicate("no-such-item").isEmpty());
    }

    void sceneQuestionsReachTheOutline()
    {
        QVERIFY(session->setField("m1n2p3q4r5", "goal", " Find the date "));
        QVERIFY(session->setField("m1n2p3q4r5", "outcome", "No, and"));
        QCOMPARE(session->fields("m1n2p3q4r5").value("goal").toString(),
                 QStringLiteral("Find the date"));
        const QVariantMap row = session->outlineOf("f6g7h8j9k0").first().toMap();
        QCOMPARE(row.value("goal").toString(), QStringLiteral("Find the date"));
        QCOMPARE(row.value("outcome").toString(), QStringLiteral("No, and"));
        QVERIFY(row.value("conflict").toString().isEmpty());
        QVERIFY(readFile("binder.toml").contains("goal = \"Find the date\""));
    }

    void nameGeneratorMakesRealAndInventedNames()
    {
        const NameGenerator names;
        const QStringList cultures = names.cultures();
        QVERIFY(cultures.contains("English") && cultures.contains("Japanese"));
        const QStringList real = names.generate("Japanese", "female", 10, false);
        QCOMPARE(real.size(), 10);
        for (const QString &name : real)
            QCOMPARE(name.count(u' '), 1);
        const QStringList invented = names.generate("Italian", "male", 8, true);
        QCOMPARE(invented.size(), 8);
        // An invented name is not a real pair from the same lists.
        const QStringList known = names.generate("Italian", "male", 2000, false);
        for (const QString &name : invented)
            QVERIFY2(!known.contains(name), qPrintable(name));
        QVERIFY(names.generate("Atlantean", "any", 5, false).isEmpty());
    }

    void boardPlacesAreKept()
    {
        QCOMPARE(session->fields("m1n2p3q4r5").value("boardX").toInt(), -1);
        QVERIFY(session->setBoardPlace("m1n2p3q4r5", 120, -5));
        QCOMPARE(session->fields("m1n2p3q4r5").value("boardX").toInt(), 120);
        QCOMPARE(session->fields("m1n2p3q4r5").value("boardY").toInt(), 0);
        QVERIFY(readFile("binder.toml").contains("board-x = 120"));
        QVERIFY(!session->setBoardPlace("no-such-item", 1, 1));
    }

    void canvasKeepsNodesAndEdges()
    {
        const QString id = session->addCanvas();
        QVERIFY(!id.isEmpty());
        QCOMPARE(session->fields(id).value("kind").toString(), QStringLiteral("canvas"));
        QVERIFY(session->canvasOf(id).value("nodes").toList().isEmpty());
        const QVariantMap canvas {
            { "nodes",
              QVariantList {
                  QVariantMap { { "id", "n1" },
                                { "x", 40 },
                                { "y", 60 },
                                { "text", "Why does the lamp\nnever go out?" } },
                  QVariantMap {
                      { "id", "n2" }, { "x", 300 }, { "y", 60 }, { "item", "m1n2p3q4r5" } } } },
            { "edges", QVariantList { QVariantMap { { "from", "n1" }, { "to", "n2" } } } }
        };
        QVERIFY(session->saveCanvas(id, canvas));
        const QVariantMap back = session->canvasOf(id);
        const QVariantList nodes = back.value("nodes").toList();
        QCOMPARE(nodes.size(), 2);
        QCOMPARE(nodes.at(0).toMap().value("text").toString(),
                 QStringLiteral("Why does the lamp\nnever go out?"));
        QCOMPARE(nodes.at(1).toMap().value("item").toString(), QStringLiteral("m1n2p3q4r5"));
        QCOMPARE(nodes.at(1).toMap().value("x").toInt(), 300);
        QCOMPARE(back.value("edges").toList().first().toMap().value("to").toString(),
                 QStringLiteral("n2"));
        // A second canvas gets its own file, and the project loads with both.
        const QString other = session->addCanvas();
        QVERIFY(session->canvasOf(other).value("nodes").toList().isEmpty());
        QCOMPARE(session->canvasOf(id).value("nodes").toList().size(), 2);
        QVERIFY(ProjectStore::load(session->folder()).ok());
        QVERIFY(!session->saveCanvas("m1n2p3q4r5", canvas));
    }

    void dailyGoalIsKept()
    {
        QVERIFY(session->setDailyGoal(750));
        QCOMPARE(session->dailyGoal(), 750);
        QVERIFY(readFile("project.toml").contains("daily = 750"));
        QVERIFY(!session->setDailyGoal(-3));
        QVERIFY(session->setDailyGoal(0));
        QCOMPARE(session->dailyGoal(), 0);
        QVERIFY(session->todayWords() >= 0);
    }

    void closingKeepsTheSession()
    {
        session->setCurrentId("m1n2p3q4r5");
        QTextDocument editor;
        session->document()->attach(&editor);
        editor.setPlainText(editor.toPlainText() + " four more new words");
        // The count of the words comes a moment after the edit.
        QTRY_VERIFY(session->sessionWords() >= 4);
        session->backup();
        session->backup();
        const QVariantList sessions = session->recentSessions(30);
        QCOMPARE(sessions.size(), 1);
        QVERIFY(sessions.first().toMap().value("words").toInt() >= 4);
    }

    void crashTextBecomesASnapshot()
    {
        session->setCurrentId("t6v4h9r2mz");
        QDir(dir.path()).mkpath(".codicier/journal");
        QFile out(dir.filePath(".codicier/journal/manuscript_the-river-office.md"));
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write("The river office, as it was before the crash.\n");
        out.close();
        QTextDocument editor;
        session->document()->attach(&editor);
        session->setCurrentId("m1n2p3q4r5");
        const QString path = session->property("recoveredSnapshot").toString();
        QVERIFY(path.startsWith("snapshots/m1n2p3q4r5/"));
        QVERIFY(path.contains("Recovered after a crash"));
        QCOMPARE(readFile(path), QByteArray("The river office, as it was before the crash.\n"));
    }

    void secondWindowOnlyReads()
    {
        QVERIFY(!session->readOnly());
        ProjectSession second;
        second.setFolder(dir.path());
        QVERIFY(second.readOnly());
        QVERIFY(second.property("openElsewhere").toBool());
        QVERIFY(second.document()->readOnly());
        QVERIFY(!second.setField("m1n2p3q4r5", "label", "green"));
        // The text of the second window never reaches the file.
        second.setCurrentId("m1n2p3q4r5");
        const QByteArray before = readFile(second.document()->file());
        QTextDocument editor;
        second.document()->attach(&editor);
        QTextCursor(&editor).insertText("Lost words. ");
        QVERIFY(!second.document()->dirty());
        QCOMPARE(readFile(second.document()->file()), before);
        // The lock goes with the first window.
        session->setFolder(QString());
        ProjectSession third;
        third.setFolder(dir.path());
        QVERIFY(!third.readOnly());
        QVERIFY(!third.property("openElsewhere").toBool());
    }

    void colourLabelsAreChecked()
    {
        QVERIFY(session->setField("m1n2p3q4r5", "label", "green"));
        QCOMPARE(session->fields("m1n2p3q4r5").value("label").toString(), QStringLiteral("green"));
        QVERIFY(readFile("binder.toml").contains("label = \"green\""));
        QVERIFY(!session->setField("m1n2p3q4r5", "label", "mauve"));
        QVERIFY(session->setField("m1n2p3q4r5", "label", ""));
        QVERIFY(session->fields("m1n2p3q4r5").value("label").toString().isEmpty());
    }

    void argumentPartsAreChecked()
    {
        QVERIFY(session->setField("m1n2p3q4r5", "argument", "claim"));
        QVERIFY(readFile("binder.toml").contains("argument = \"claim\""));
        QVERIFY(!session->setField("m1n2p3q4r5", "argument", "opinion"));
        QVERIFY(session->setField("m1n2p3q4r5", "argument", ""));
        QVERIFY(!readFile("binder.toml").contains("argument ="));
    }

    void argumentMapLinksEvidenceToClaims()
    {
        QStringList scenes;
        QString note;
        for (const QVariant &each : session->items()) {
            const QVariantMap item = each.toMap();
            // Any item can hold a part of the argument.
            if (item.value("kind") != "note")
                scenes.append(item.value("id").toString());
            else if (note.isEmpty())
                note = item.value("id").toString();
        }
        QVERIFY(scenes.size() >= 3);
        QVERIFY(!note.isEmpty());
        QVERIFY(session->setField(scenes[0], "argument", "thesis"));
        QVERIFY(session->setField(scenes[1], "argument", "claim"));
        QVERIFY(session->setField(scenes[2], "argument", "claim"));
        QVERIFY(session->setField(note, "argument", "evidence"));
        QVERIFY(session->setField(note, "supports", QStringList { scenes[1] }));
        QVERIFY(readFile("binder.toml").contains("supports = ["));

        QVariantMap map = session->argumentMap();
        QCOMPARE(map.value("thesis").toList().size(), 1);
        const QVariantList claims = map.value("claims").toList();
        QCOMPARE(claims.size(), 2);
        QCOMPARE(claims[0].toMap().value("id").toString(), scenes[1]);
        QCOMPARE(claims[0].toMap().value("evidence").toList().size(), 1);
        QVERIFY(claims[1].toMap().value("evidence").toList().isEmpty());
        QVERIFY(map.value("loose").toList().isEmpty());
        QCOMPARE(session->fields(note).value("supports").toStringList(), QStringList { scenes[1] });

        // Evidence that supports no claim is loose.
        QVERIFY(session->setField(note, "supports", QStringList()));
        map = session->argumentMap();
        QCOMPARE(map.value("loose").toList().size(), 1);
    }

    void researchTakesACopyOfAnyFile()
    {
        const QUrl pdf = QUrl::fromLocalFile(QStringLiteral(FIXTURE_DIR "/research/salt.pdf"));
        const QString id = session->addResearch(pdf);
        QVERIFY(!id.isEmpty());
        QCOMPARE(session->currentId(), id);
        QCOMPARE(session->fields(id).value("kind").toString(), QStringLiteral("file"));
        const QString file = session->fileUrl(id).toLocalFile();
        QVERIFY(file.endsWith("/research/salt.pdf"));
        QCOMPARE(QFileInfo(file).size(), QFileInfo(pdf.toLocalFile()).size());
        // A second copy gets a name of its own.
        QVERIFY(session->fileUrl(session->addResearch(pdf))
                    .toLocalFile()
                    .endsWith("/research/salt-2.pdf"));
        QVERIFY(session->addResearch(QUrl::fromLocalFile("/no/such/file.pdf")).isEmpty());
        QVERIFY(!session->lastError().isEmpty());
    }

    void researchLinksShowOnBothSides()
    {
        const QString file = session->addResearch(
            QUrl::fromLocalFile(QStringLiteral(FIXTURE_DIR "/research/salt.pdf")));
        QVERIFY(session->researchOf("m1n2p3q4r5").isEmpty());
        QVERIFY(session->setField(file, "texts", QStringList { "m1n2p3q4r5" }));
        QCOMPARE(session->fields(file).value("texts").toStringList(), QStringList { "m1n2p3q4r5" });
        const QVariantList research = session->researchOf("m1n2p3q4r5");
        QCOMPARE(research.size(), 1);
        QCOMPARE(research.first().toMap().value("id").toString(), file);
        // A research file in the trash shows no link.
        QVERIFY(session->moveToTrash(file));
        QVERIFY(session->researchOf("m1n2p3q4r5").isEmpty());
    }

    void submissionsRecordWhereAPieceWent()
    {
        QVERIFY(session->submissions().isEmpty());
        QVERIFY(!session->addSubmission("m1n2p3q4r5", "  ", "2026-10-01"));
        QVERIFY(!session->addSubmission("m1n2p3q4r5", "The Review", "1 October"));
        QVERIFY(!session->addSubmission("nothing", "The Review", ""));
        QVERIFY(session->addSubmission("m1n2p3q4r5", "The Review", "2026-10-01"));
        QVERIFY(session->addSubmission("", "A Press", ""));
        QVERIFY(readFile("binder.toml").contains("[[submission]]"));
        QVariantList list = session->submissions();
        QCOMPARE(list.size(), 2);
        QCOMPARE(list[0].toMap().value("venue").toString(), QStringLiteral("The Review"));
        QCOMPARE(list[0].toMap().value("sent").toString(), QStringLiteral("2026-10-01"));
        QCOMPARE(list[1].toMap().value("title").toString(), session->title());
        QCOMPARE(list[1].toMap().value("sent").toString(),
                 QDate::currentDate().toString(Qt::ISODate));

        QVERIFY(session->setSubmissionResult(0, "accepted"));
        QVERIFY(!session->setSubmissionResult(0, "lost"));
        QCOMPARE(session->submissions()[0].toMap().value("result").toString(),
                 QStringLiteral("accepted"));
        QVERIFY(session->setSubmissionResult(0, ""));
        QVERIFY(session->submissions()[0].toMap().value("result").toString().isEmpty());
        QVERIFY(session->removeSubmission(0));
        QVERIFY(session->removeSubmission(0));
        QVERIFY(!readFile("binder.toml").contains("submission"));
        QVERIFY(!session->removeSubmission(0));
    }

    void printChaptersLeavesOutTheLeftOutScenes()
    {
        const QVariantList all = session->printChapters(QString());
        QVERIFY(!all.isEmpty());
        int scenes = 0;
        for (const QVariant &chapter : all)
            scenes += static_cast<int>(chapter.toMap().value("texts").toStringList().size());
        // One chapter for a scene id; a scene left out is not in it.
        const QVariantList one = session->printChapters("m1n2p3q4r5");
        QCOMPARE(one.size(), 1);
        QVERIFY(session->setField("m1n2p3q4r5", "include", false));
        int after = 0;
        for (const QVariant &chapter : session->printChapters(QString()))
            after += static_cast<int>(chapter.toMap().value("texts").toStringList().size());
        QCOMPARE(after, scenes - 1);
    }

    void aSeriesGivesItsBooksOneByOne()
    {
        QTemporaryDir series;
        QVERIFY(Templates::create(series.filePath("saga"), "series", "Saga", QString()).isEmpty());
        ProjectSession saga;
        saga.setFolder(series.filePath("saga"));
        // One book is no series.
        QVERIFY(saga.books().isEmpty());
        QString first;
        for (const QVariant &item : saga.items()) {
            if (item.toMap().value("kind").toString() == QLatin1String("folder"))
                first = item.toMap().value("id").toString();
        }
        QVERIFY(!first.isEmpty());
        saga.setCurrentId(QString());
        const QString second = saga.add("folder");
        QVERIFY(!second.isEmpty());
        QVERIFY(saga.rename(second, "Book Two"));
        QVERIFY(saga.books().isEmpty());
        saga.setCurrentId(second);
        QVERIFY(!saga.add("chapter").isEmpty());
        QVERIFY(!saga.add("scene").isEmpty());
        const QVariantList books = saga.books();
        QCOMPARE(books.size(), 2);
        QCOMPARE(books.at(1).toMap().value("title").toString(), QStringLiteral("Book Two"));
        QCOMPARE(saga.printChapters(first).size(), 1);
        QCOMPARE(saga.printChapters(second).size(), 1);
        QCOMPARE(saga.printChapters(QString()).size(), 2);
    }

    void insightsListWhatIsMissing()
    {
        const auto idsOf = [](const QVariantList &list) {
            QStringList ids;
            for (const QVariant &item : list)
                ids.append(item.toMap().value("id").toString());
            return ids;
        };
        const QVariantMap before = session->insights();
        const QVariantList chapters = before.value("chapters").toList();
        QVERIFY(!chapters.isEmpty());
        const QVariantMap chapter = chapters.first().toMap();
        const QString sceneId
            = chapter.value("scenes").toList().first().toMap().value("id").toString();
        QVERIFY(session->setField(sceneId, "pov", QString()));
        QVERIFY(session->setField(sceneId, "synopsis", QString()));
        QVERIFY(session->setField(sceneId, "date", QString()));
        QVERIFY(idsOf(session->insights().value("gaps").toList()).contains(sceneId));
        QVERIFY(session->setField(sceneId, "pov", QStringLiteral("Mara")));
        QVERIFY(session->setField(sceneId, "synopsis", QStringLiteral("She arrives.")));
        QVERIFY(session->setField(sceneId, "date", QStringLiteral("2026-01-01")));
        QVERIFY(!idsOf(session->insights().value("gaps").toList()).contains(sceneId));
        // A scene in draft in a chapter marked final is listed.
        QVERIFY(session->setField(sceneId, "status", QStringLiteral("draft")));
        QVERIFY(
            session->setField(chapter.value("id").toString(), "status", QStringLiteral("final")));
        QVERIFY(idsOf(session->insights().value("unfinished").toList()).contains(sceneId));
        QVERIFY(session->setField(sceneId, "status", QStringLiteral("final")));
        QVERIFY(!idsOf(session->insights().value("unfinished").toList()).contains(sceneId));
    }

    void suggestedEntriesAreNamesThatComeBackAndHaveNoEntry()
    {
        session->setCurrentId("m1n2p3q4r5");
        QTextDocument editor;
        session->document()->attach(&editor);
        QString body;
        for (int i = 0; i < 4; ++i)
            body += QStringLiteral("Then we saw Zelda and Port Ruin. We waited for Quill. ");
        // A name at the start of a sentence, and a name seen once, are no sign.
        body += QStringLiteral("Quartz is rare. We met Oddo once.");
        editor.setPlainText(body);
        const QVariantList found = session->suggestedEntries();
        QStringList names;
        for (const QVariant &each : found)
            names.append(each.toMap().value("name").toString());
        QVERIFY(names.contains("Zelda"));
        QVERIFY(names.contains("Port Ruin"));
        QVERIFY(names.contains("Quill"));
        QVERIFY(!names.contains("Then"));
        QVERIFY(!names.contains("Quartz"));
        QVERIFY(!names.contains("Oddo"));
        // A name that the codex holds is not offered again.
        const QString id = session->add("character");
        QVERIFY(session->rename(id, "Zelda"));
        session->setCurrentId("m1n2p3q4r5");
        QStringList after;
        for (const QVariant &each : session->suggestedEntries())
            after.append(each.toMap().value("name").toString());
        QVERIFY(!after.contains("Zelda"));
        QVERIFY(after.contains("Quill"));
    }

    void firstAppearanceIsTheEarliestSceneThatNamesTheEntity()
    {
        session->setCurrentId("m1n2p3q4r5");
        QTextDocument editor;
        session->document()->attach(&editor);
        editor.setPlainText(QStringLiteral("Only Zelda knew."));
        const QString zelda = session->add("character");
        QVERIFY(session->rename(zelda, "Zelda"));
        const QString nobody = session->add("character");
        QVERIFY(session->rename(nobody, "Nobody Known"));
        session->setCurrentId("m1n2p3q4r5");
        const QVariantMap first = session->firstAppearances();
        QCOMPARE(first.value(zelda).toString(),
                 session->fields("m1n2p3q4r5").value("title").toString());
        QVERIFY(!first.contains(nobody));
    }

    void wholeManuscriptForTheBoardAndOutliner()
    {
        int scenes = 0;
        for (const QVariant &chapter : session->printChapters(QString()))
            scenes += static_cast<int>(chapter.toMap().value("texts").toStringList().size());
        // Every scene, also one left out of the print, is on the board.
        QVERIFY(session->childrenOf("@manuscript").size() >= scenes);
        for (const QVariant &card : session->childrenOf("@manuscript"))
            QCOMPARE(card.toMap().value("kind").toString(), QStringLiteral("scene"));
        QVERIFY(session->outlineOf("@manuscript").size()
                >= session->childrenOf("@manuscript").size());
        QVERIFY(session->childrenOf("no-such-id").isEmpty());
    }

    void numbersFollowTheBinder()
    {
        QVERIFY(!session->numbered());
        QVERIFY(session->setNumbered(true));
        QVERIFY(readFile("project.toml").contains("numbered = true"));
        const QVariantMap numbers = session->numbers();
        // The chapter "The Letter" is the first chapter, and its scenes are 1.1 and 1.2.
        QCOMPARE(numbers.value("f6g7h8j9k0").toString(), QStringLiteral("1"));
        QCOMPARE(numbers.value("m1n2p3q4r5").toString(), QStringLiteral("1.1"));
        QCOMPARE(numbers.value("t6v4h9r2mz").toString(), QStringLiteral("1.2"));
        QVERIFY(!numbers.contains("e5f6g7h8j9"));
        QVERIFY(session->setNumbered(false));
        QVERIFY(!readFile("project.toml").contains("numbered"));
    }

    void sceneBreakIsAProjectChoice()
    {
        QCOMPARE(session->sceneBreak(), QStringLiteral("***"));
        QVERIFY(!session->setSceneBreak("~~~"));
        QVERIFY(session->setSceneBreak("#"));
        QCOMPARE(session->sceneBreak(), QStringLiteral("#"));
        QVERIFY(readFile("project.toml").contains("scene-break = \"#\""));
        QVERIFY(session->setSceneBreak(""));
        QCOMPARE(session->sceneBreak(), QString());
    }

    void beatBoardPlacesScenesOnBeats()
    {
        QVERIFY(session->beatBoard().isEmpty());
        QVERIFY(!session->setFramework("no-such-framework"));
        QVERIFY(session->setFramework("kishotenketsu"));
        QCOMPARE(session->framework(), QStringLiteral("kishotenketsu"));
        QVERIFY(readFile("project.toml").contains("framework = \"kishotenketsu\""));
        QVERIFY(session->setField("m1n2p3q4r5", "beat", "ten"));
        QCOMPARE(session->fields("m1n2p3q4r5").value("beat").toString(), QStringLiteral("ten"));

        const QVariantList columns = session->beatBoard();
        QCOMPARE(columns.size(), 5);
        QCOMPARE(columns.first().toMap().value("name").toString(), QStringLiteral("No beat"));
        QCOMPARE(columns.first().toMap().value("scenes").toList().size(), 1);
        const QVariantMap ten = columns.at(3).toMap();
        QCOMPARE(ten.value("id").toString(), QStringLiteral("ten"));
        QCOMPARE(ten.value("scenes").toList().first().toMap().value("id").toString(),
                 QStringLiteral("m1n2p3q4r5"));

        // A beat of another framework counts as no beat.
        QVERIFY(session->setFramework("three-act"));
        QCOMPARE(session->beatBoard().first().toMap().value("scenes").toList().size(), 2);
        QVERIFY(session->setField("m1n2p3q4r5", "beat", ""));
        QVERIFY(session->fields("m1n2p3q4r5").value("beat").toString().isEmpty());
    }

    void plotlinesHoldScenes()
    {
        QVERIFY(session->plotlines().isEmpty());
        QCOMPARE(session->addPlotline(" The Romance "), QStringLiteral("the-romance"));
        QCOMPARE(session->addPlotline("The romance!"), QStringLiteral("the-romance-2"));
        QCOMPARE(session->addPlotline("???"), QStringLiteral("plotline"));
        QVERIFY(session->addPlotline("  ").isEmpty());
        QCOMPARE(session->plotlines().first().toMap().value("name").toString(),
                 QStringLiteral("The Romance"));
        QVERIFY(session->setField("m1n2p3q4r5", "plotlines",
                                  QStringList { "the-romance", "plotline" }));
        QCOMPARE(session->fields("m1n2p3q4r5").value("plotlines").toStringList(),
                 (QStringList { "the-romance", "plotline" }));
        // Both files load again and come back byte for byte.
        const LoadResult result = ProjectStore::load(dir.path());
        QVERIFY(result.ok());
        QCOMPARE(ProjectStore::projectFile(result.project), readFile("project.toml"));
        QCOMPARE(ProjectStore::binderFile(result.project), readFile("binder.toml"));
    }

    void plotGridListsTheScenes()
    {
        const QString line = session->addPlotline("The ledger");
        QVERIFY(session->setField("t6v4h9r2mz", "plotlines", QStringList { line }));
        const QVariantList scenes = session->plotGrid();
        QCOMPARE(scenes.size(), 2);
        QCOMPARE(scenes.first().toMap().value("id").toString(), QStringLiteral("m1n2p3q4r5"));
        QVERIFY(scenes.first().toMap().value("plotlines").toStringList().isEmpty());
        QCOMPARE(scenes.at(1).toMap().value("plotlines").toStringList(), QStringList { line });
    }

    void storyDatesAreChecked()
    {
        QVERIFY(session->setField("t6v4h9r2mz", "date", " 1847-03-04 "));
        QCOMPARE(session->fields("t6v4h9r2mz").value("date").toString(),
                 QStringLiteral("1847-03-04"));
        QCOMPARE(session->fields("t6v4h9r2mz").value("dateLabel").toString(),
                 QStringLiteral("4 March 1847"));
        QVERIFY(!session->setField("t6v4h9r2mz", "date", "1847-02-30"));
        QCOMPARE(session->fields("t6v4h9r2mz").value("date").toString(),
                 QStringLiteral("1847-03-04"));
        QVERIFY(readFile("binder.toml").contains("date = \"1847-03-04\""));
        QVERIFY(session->setField("t6v4h9r2mz", "date", ""));
        QVERIFY(!readFile("binder.toml").contains("date = "));
    }

    void timelineGroupsDatedItems()
    {
        QVERIFY(session->timeline("pov").isEmpty());
        QVERIFY(session->setField("t6v4h9r2mz", "date", "1847-03-04"));
        QVERIFY(session->setField("m1n2p3q4r5", "date", "1847-03-01"));
        const QString line = session->addPlotline("The ledger");
        QVERIFY(session->setField("t6v4h9r2mz", "plotlines", QStringList { line }));

        // Both scenes have the POV Mara, the earlier one first.
        const QVariantList byPov = session->timeline("pov");
        QCOMPARE(byPov.size(), 1);
        const QVariantMap mara = byPov.first().toMap();
        QCOMPARE(mara.value("name").toString(), QStringLiteral("Mara"));
        const QVariantList events = mara.value("events").toList();
        QCOMPARE(events.size(), 2);
        QCOMPARE(events.first().toMap().value("id").toString(), QStringLiteral("m1n2p3q4r5"));
        QCOMPARE(events.first().toMap().value("label").toString(), QStringLiteral("1 March 1847"));

        const QVariantList byLine = session->timeline("plotline");
        QStringList names;
        for (const QVariant &track : byLine)
            names.append(track.toMap().value("name").toString());
        QCOMPARE(names, (QStringList { "No plotline", "The ledger" }));

        // An item in the trash leaves the timeline.
        QVERIFY(session->moveToTrash("m1n2p3q4r5"));
        QCOMPARE(session->timeline("pov").first().toMap().value("events").toList().size(), 1);
    }

    void customCalendarSetsTheDates()
    {
        QVERIFY(session->calendarMonths().isEmpty());
        QVERIFY(!session->setCalendar({ QVariantMap { { "name", "Bad" }, { "days", 0 } } }));
        QVERIFY(session->setCalendar({ QVariantMap { { "name", "Frostmonth" }, { "days", 40 } },
                                       QVariantMap { { "name", "Thaw" }, { "days", 20 } } }));
        QCOMPARE(session->calendarMonths().size(), 2);
        QVERIFY(session->setField("t6v4h9r2mz", "date", "312-1-40"));
        QCOMPARE(session->fields("t6v4h9r2mz").value("dateLabel").toString(),
                 QStringLiteral("40 Frostmonth 312"));
        QVERIFY(!session->setField("t6v4h9r2mz", "date", "312-3-1"));
        QCOMPARE(session->timeline("pov")
                     .first()
                     .toMap()
                     .value("events")
                     .toList()
                     .first()
                     .toMap()
                     .value("label")
                     .toString(),
                 QStringLiteral("40 Frostmonth 312"));
        // Both files load again and come back byte for byte.
        const LoadResult result = ProjectStore::load(dir.path());
        QVERIFY(result.ok());
        QCOMPARE(ProjectStore::projectFile(result.project), readFile("project.toml"));
        // No months is the Gregorian calendar again.
        QVERIFY(session->setCalendar({ }));
        QVERIFY(!readFile("project.toml").contains("[calendar"));
    }

    void relationsLinkEntities()
    {
        const QString mara = session->add("character");
        QVERIFY(session->rename(mara, "Mara"));
        const QString mother = session->add("character");
        QVERIFY(session->rename(mother, "Ilse"));
        const QString port = session->add("place");
        QVERIFY(!session->addRelation(mara, mara, "self", false));
        QVERIFY(!session->addRelation(mara, "t6v4h9r2mz", "scene", false));
        QVERIFY(!session->addRelation(mara, port, " ", false));
        QVERIFY(session->addRelation(mara, port, " lives in ", false));
        QVERIFY(session->addRelation(mother, mara, "ignored", true));

        const QVariantList own = session->relations(mara);
        QCOMPARE(own.size(), 2);
        QCOMPARE(own.first().toMap().value("name").toString(), QStringLiteral("lives in"));
        const QVariantMap parent = own.at(1).toMap();
        QCOMPARE(parent.value("name").toString(), QStringLiteral("parent of"));
        QVERIFY(parent.value("parent").toBool());
        QCOMPARE(parent.value("fromTitle").toString(), QStringLiteral("Ilse"));
        QCOMPARE(session->relations(port).size(), 1);

        // Both files load again and come back byte for byte.
        const LoadResult result = ProjectStore::load(dir.path());
        QVERIFY(result.ok());
        QCOMPARE(ProjectStore::binderFile(result.project), readFile("binder.toml"));

        // A relation to an item in the trash does not show.
        QVERIFY(session->moveToTrash(port));
        QCOMPARE(session->relations(mara).size(), 1);
        QVERIFY(session->removeRelation(parent.value("index").toInt()));
        QVERIFY(session->relations(mara).isEmpty());
        QVERIFY(!session->removeRelation(7));
    }

    void familyTreeFromParentLinks()
    {
        QStringList ids;
        for (const char *name : { "Old Vell", "Ilse", "Mara", "Abel" }) {
            ids.append(session->add("character"));
            QVERIFY(session->rename(ids.last(), name));
        }
        QVERIFY(session->addRelation(ids[0], ids[1], { }, true));
        QVERIFY(session->addRelation(ids[1], ids[2], { }, true));
        // A grandparent link puts Mara one row below Ilse, not one row below Old Vell.
        QVERIFY(session->addRelation(ids[0], ids[2], { }, true));
        QVERIFY(session->addRelation(ids[2], ids[3], "rival of", false));

        const QVariantMap tree = session->familyTree();
        const QVariantList generations = tree.value("generations").toList();
        QCOMPARE(generations.size(), 3);
        QCOMPARE(generations.at(2).toList().first().toMap().value("title").toString(),
                 QStringLiteral("Mara"));
        QCOMPARE(tree.value("links").toList().size(), 3);

        // A loop of parent links does not hang the tree.
        QVERIFY(session->addRelation(ids[2], ids[0], { }, true));
        QVERIFY(!session->familyTree().value("generations").toList().isEmpty());
    }

    void mapsCopyTheImageIntoTheProject()
    {
        QTemporaryDir source;
        QImage image(40, 30, QImage::Format_RGB32);
        image.fill(Qt::darkCyan);
        QVERIFY(image.save(source.filePath("The Harbour.png")));
        QFile text(source.filePath("notes.txt"));
        QVERIFY(text.open(QIODevice::WriteOnly));
        text.close();

        QVERIFY(session->addMap(QUrl::fromLocalFile(source.filePath("notes.txt"))).isEmpty());
        QVERIFY(!session->lastError().isEmpty());
        const QString id = session->addMap(QUrl::fromLocalFile(source.filePath("The Harbour.png")));
        QVERIFY(!id.isEmpty());
        const ProjectItem map = onDisk(dir.path(), id);
        QCOMPARE(map.kind, QStringLiteral("map"));
        QCOMPARE(map.section, QStringLiteral("codex"));
        QCOMPARE(map.title, QStringLiteral("The Harbour"));
        QCOMPARE(map.file, QStringLiteral("maps/the-harbour.png"));
        QVERIFY(QImage(QDir(dir.path()).filePath(map.file)).size() == QSize(40, 30));
        QCOMPARE(session->fileUrl(id), QUrl::fromLocalFile(QDir(dir.path()).filePath(map.file)));
        // A second copy gets a new file name.
        const QString again
            = session->addMap(QUrl::fromLocalFile(source.filePath("The Harbour.png")));
        QCOMPARE(onDisk(dir.path(), again).file, QStringLiteral("maps/the-harbour-2.png"));
        // The image is not a text: the editor does not open it, and it has no words.
        QCOMPARE(session->document()->file(), QString());
        QCOMPARE(session->binder()->wordsOf(id), 0);
        QVERIFY(session->search("harbour").size() >= 1);
    }

    void pinsPlacePlacesOnAMap()
    {
        QTemporaryDir source;
        QImage image(20, 10, QImage::Format_RGB32);
        image.fill(Qt::white);
        QVERIFY(image.save(source.filePath("map.png")));
        const QString map = session->addMap(QUrl::fromLocalFile(source.filePath("map.png")));
        const QString port = session->add("place");
        QVERIFY(session->rename(port, "Port"));
        QVERIFY(!session->addPin(map, port, 1.5, 0.5));
        QVERIFY(!session->addPin(port, port, 0.5, 0.5));
        QVERIFY(!session->addPin(map, "t6v4h9r2mz", 0.5, 0.5));
        QVERIFY(session->addPin(map, port, 0.25, 0.75));
        QVariantList pins = session->pins(map);
        QCOMPARE(pins.size(), 1);
        QCOMPARE(pins.first().toMap().value("title").toString(), QStringLiteral("Port"));
        QCOMPARE(pins.first().toMap().value("x").toDouble(), 0.25);
        QVERIFY(session->movePin(0, 0.5, 0.5));
        QVERIFY(!session->movePin(0, -1, 0.5));
        QCOMPARE(session->pins(map).first().toMap().value("y").toDouble(), 0.5);

        const LoadResult result = ProjectStore::load(dir.path());
        QVERIFY(result.ok());
        QCOMPARE(ProjectStore::binderFile(result.project), readFile("binder.toml"));

        QVERIFY(session->moveToTrash(port));
        QVERIFY(session->pins(map).isEmpty());
        QVERIFY(session->removePin(0));
        QVERIFY(!session->removePin(0));
    }

    void insightsGiveTheLengths()
    {
        const QVariantList chapters = session->insights().value("chapters").toList();
        QCOMPARE(chapters.size(), 1);
        const QVariantMap letter = chapters.first().toMap();
        QCOMPARE(letter.value("title").toString(), QStringLiteral("The Letter"));
        QCOMPARE(letter.value("words").toInt(), 28);
        const QVariantList scenes = letter.value("scenes").toList();
        QCOMPARE(scenes.size(), 2);
        QCOMPARE(scenes.at(1).toMap().value("words").toInt(), 18);
    }

    void insightsGivePovAndDialogue()
    {
        QVERIFY(session->setField("m1n2p3q4r5", "pov", "Abel"));
        session->setCurrentId("m1n2p3q4r5");
        QTextDocument editor;
        session->document()->attach(&editor);
        editor.setPlainText(
            "“Not here,” said Abel. \"Not anymore,\" he said again, and then he left.");
        const QVariantMap figures = session->insights();
        const QVariantMap letter = figures.value("chapters").toList().first().toMap();
        const QVariantMap pov = letter.value("pov").toMap();
        QCOMPARE(pov.value("Mara").toInt(), 18);
        QCOMPARE(pov.value("Abel").toInt(), session->binder()->wordsOf("m1n2p3q4r5"));
        QCOMPARE(figures.value("povNames").toStringList().first(), QStringLiteral("Mara"));
        // Four words in quotes: "Not here" and "Not anymore".
        const int words = letter.value("words").toInt();
        QCOMPARE(letter.value("dialogue").toDouble(), 4.0 / words);
    }

    void insightsKeepNamesApart()
    {
        const QString salt = session->add("character");
        QVERIFY(session->rename(salt, "Salt Vane"));
        QVERIFY(session->setField(salt, "aliases", QStringList { "the clerk" }));
        session->setCurrentId("m1n2p3q4r5");
        QTextDocument editor;
        session->document()->attach(&editor);
        editor.setPlainText("Salt Vane wrote. The clerk wrote. Vane wrote again.");
        const QVariantMap figures = session->insights();
        QStringList words;
        for (const QVariant &each : figures.value("words").toList())
            words.append(each.toMap().value("word").toString());
        QVERIFY(words.contains("wrote"));
        QVERIFY(!words.contains("salt") && !words.contains("vane") && !words.contains("clerk"));
        const QVariantMap first = figures.value("names").toList().first().toMap();
        QCOMPARE(first.value("word").toString(), QStringLiteral("Salt Vane"));
        QCOMPARE(first.value("count").toInt(), 2);
    }

    void repeatedPhrasesAcrossScenes()
    {
        session->setCurrentId("m1n2p3q4r5");
        QTextDocument editor;
        session->document()->attach(&editor);
        editor.setPlainText("Her heart beat like a drum. She was in the room.");
        session->setCurrentId("t6v4h9r2mz");
        editor.setPlainText("Again her heart beat like a drum, slow. She was in the hall.");
        const QVariantList phrases = session->repeatedPhrases();
        QVERIFY(!phrases.isEmpty());
        const QVariantMap first = phrases.first().toMap();
        // The longest form wins over its parts, as it comes as often.
        QCOMPARE(first.value("phrase").toString(), QStringLiteral("her heart beat like a drum"));
        QCOMPARE(first.value("count").toInt(), 2);
        QCOMPARE(first.value("places").toList().size(), 2);
        QCOMPARE(first.value("places").toList().first().toMap().value("id").toString(),
                 QStringLiteral("m1n2p3q4r5"));
        // Common words alone do not count: "she was in the".
        for (const QVariant &each : phrases)
            QVERIFY(each.toMap().value("phrase").toString() != "she was in the");
    }

    void insightsGiveSentenceLengthAndEase()
    {
        session->setCurrentId("m1n2p3q4r5");
        QTextDocument editor;
        session->document()->attach(&editor);
        // Two sentences of four words: 8 words, 2 sentences, 8 syllables.
        editor.setPlainText("The cat sat down. The dog ran off!");
        session->setCurrentId("t6v4h9r2mz");
        editor.setPlainText("");
        const QVariantMap chapter = session->insights().value("chapters").toList().first().toMap();
        QCOMPARE(chapter.value("sentenceLength").toDouble(), 4.0);
        // 206.835 - 1.015 * 4 - 84.6 * 1 = 118.175
        QVERIFY(qAbs(chapter.value("readingEase").toDouble() - 118.175) < 0.01);
    }

    void trackedChangesAreAcceptedOrRejected()
    {
        session->setCurrentId("m1n2p3q4r5");
        QTextDocument editor;
        session->document()->attach(&editor);
        editor.setPlainText("The cat sat on the mat.");
        QVERIFY(!session->tracking());
        QVERIFY(session->startTracking());
        QVERIFY(session->tracking());
        QTextCursor cursor(&editor);
        cursor.select(QTextCursor::Document);
        cursor.insertText("The dog sat on the red mat.");
        const QVariantList list = session->changes();
        QCOMPARE(list.size(), 2);
        QCOMPARE(list.at(0).toMap().value("removed").toString().trimmed(), QStringLiteral("cat"));
        QCOMPARE(list.at(0).toMap().value("added").toString().trimmed(), QStringLiteral("dog"));
        QCOMPARE(list.at(1).toMap().value("added").toString().trimmed(), QStringLiteral("red"));
        // Reject puts the old words back in the page.
        QVERIFY(session->rejectChange(1));
        QCOMPARE(editor.toPlainText(), QStringLiteral("The dog sat on the mat."));
        // Accept puts the change into the baseline, so no change is left.
        QVERIFY(session->acceptChange(0));
        QVERIFY(session->changes().isEmpty());
        QVERIFY(!session->acceptChange(0));
        QVERIFY(session->acceptAll());
        QVERIFY(!session->tracking());
    }

    void insightsGiveTheMostUsedWords()
    {
        session->setCurrentId("m1n2p3q4r5");
        QTextDocument editor;
        session->document()->attach(&editor);
        editor.setPlainText("The ledger, the ledger, and the LEDGER. Salt salt. A cat sat.");
        const QVariantList words = session->insights().value("words").toList();
        QCOMPARE(words.first().toMap().value("word").toString(), QStringLiteral("ledger"));
        QCOMPARE(words.first().toMap().value("count").toInt(), 3);
        QStringList all;
        for (const QVariant &each : words)
            all.append(each.toMap().value("word").toString());
        // No stop word, and no word of two letters or less.
        QVERIFY(!all.contains("the"));
        QVERIFY(!all.contains("and"));
        QVERIFY(!all.contains("a"));
        QVERIFY(all.contains("salt"));
        QVERIFY(words.size() <= 30);
    }

    void insightsCountCharactersInEachChapter()
    {
        const QString mara = session->add("character");
        QVERIFY(session->rename(mara, "Mara"));
        QVERIFY(session->setField(mara, "aliases", QStringList { "the clerk" }));
        const QString abel = session->add("character");
        QVERIFY(session->rename(abel, "Abel"));
        session->setCurrentId("m1n2p3q4r5");
        QTextDocument editor;
        session->document()->attach(&editor);
        editor.setPlainText("Mara met the clerk. Marathon. Mara again.");
        const QVariantMap figures = session->insights();
        const QVariantList people = figures.value("characters").toList();
        QCOMPARE(people.size(), 2);
        QCOMPARE(people.first().toMap().value("id").toString(), mara);
        const QVariantList presence
            = figures.value("chapters").toList().first().toMap().value("presence").toList();
        // Two times "Mara" and one time "the clerk"; "Marathon" does not count.
        QCOMPARE(presence.at(0).toInt(), 3);
        QCOMPARE(presence.at(1).toInt(), 0);
    }

    void passesColorTheTextOfEachPass()
    {
        QVERIFY(session->passes().isEmpty());
        QCOMPARE(session->startPass("  "), 0);
        QCOMPARE(session->startPass("Structure"), 1);
        QCOMPARE(session->passes().first().toMap().value("name").toString(),
                 QStringLiteral("Structure"));
        session->setCurrentId("t6v4h9r2mz");
        QTextDocument editor;
        session->document()->attach(&editor);
        QTextCursor cursor(&editor);
        cursor.insertText("Rain. ");
        QCOMPARE(session->startPass("Line edit"), 2);
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(" Snow.");

        const QString now = editor.toPlainText();
        QVariantList ranges = session->passRanges("t6v4h9r2mz");
        QCOMPARE(ranges.size(), 2);
        const QVariantMap first = ranges.first().toMap();
        QCOMPARE(first.value("pass").toInt(), 1);
        QCOMPARE(now.mid(first.value("start").toInt(), first.value("length").toInt()).trimmed(),
                 QStringLiteral("Rain."));
        const QVariantMap second = ranges.last().toMap();
        QCOMPARE(second.value("pass").toInt(), 2);
        QCOMPARE(now.mid(second.value("start").toInt(), second.value("length").toInt()).trimmed(),
                 QStringLiteral("Snow."));

        // A text from before the first pass has no tint at all.
        QVERIFY(session->passRanges("m1n2p3q4r5").isEmpty());
        QVERIFY(readFile("project.toml").contains("name = \"Line edit\""));
        session->setShowPasses(true);
        QVERIFY(session->showPasses());
    }

    void commentsFollowTheirQuote()
    {
        QVERIFY(!session->addComment("t6v4h9r2mz", 0, "", "Empty quote"));
        QVERIFY(!session->addComment("t6v4h9r2mz", 4, "letter", " "));
        QVERIFY(session->addComment("t6v4h9r2mz", 4, "letter", " Which letter? "));
        QVariantList comments = session->comments();
        QCOMPARE(comments.size(), 1);
        QVariantMap comment = comments.first().toMap();
        QCOMPARE(comment.value("text").toString(), QStringLiteral("Which letter?"));
        QCOMPARE(comment.value("start").toInt(), 4);
        QCOMPARE(comment.value("title").toString(), QStringLiteral("A Date She Did Not Know"));

        // New words before the quote move it, and the comment follows.
        session->setCurrentId("t6v4h9r2mz");
        QTextDocument editor;
        session->document()->attach(&editor);
        QTextCursor(&editor).insertText("Rain. ");
        QCOMPARE(session->comments("t6v4h9r2mz").first().toMap().value("start").toInt(), 10);

        // Both files load again and come back byte for byte.
        QVERIFY(session->resolveComment(0, true));
        QVERIFY(session->comments().first().toMap().value("resolved").toBool());
        const LoadResult result = ProjectStore::load(dir.path());
        QVERIFY(result.ok());
        QCOMPARE(ProjectStore::binderFile(result.project), readFile("binder.toml"));
        QVERIFY(session->resolveComment(0, false));
        QVERIFY(!readFile("binder.toml").contains("resolved"));

        // A quote that is gone gives -1.
        editor.setPlainText("Nothing of it.");
        QCOMPARE(session->comments().first().toMap().value("start").toInt(), -1);
        QVERIFY(session->removeComment(0));
        QVERIFY(session->comments().isEmpty());
        QVERIFY(!session->removeComment(0));
    }

    void checklistsTickChecksForChapters()
    {
        QVERIFY(!session->setPassChecks(1, { "Too soon" }));
        QCOMPARE(session->startPass("Structure"), 1);
        QVERIFY(session->setPassChecks(
            1, { " Cut filter words ", "", "Check the POV", "Check the POV" }));
        QVariantMap list = session->checklist(1);
        QCOMPARE(list.value("checks").toStringList(),
                 (QStringList { "Cut filter words", "Check the POV" }));
        const QVariantMap letter = list.value("chapters").toList().first().toMap();
        QCOMPARE(letter.value("id").toString(), QStringLiteral("f6g7h8j9k0"));
        QCOMPARE(letter.value("ticked").toList(), (QVariantList { false, false }));

        QVERIFY(session->setTick(1, "Check the POV", "f6g7h8j9k0", true));
        QVERIFY(session->setTick(1, "Check the POV", "f6g7h8j9k0", true));
        list = session->checklist(1);
        QCOMPARE(list.value("chapters").toList().first().toMap().value("ticked").toList(),
                 (QVariantList { false, true }));
        // A tick of pass 1 is not a tick of pass 2.
        QCOMPARE(session->startPass("Line edit"), 2);
        QVERIFY(session->setPassChecks(2, { "Check the POV" }));
        QCOMPARE(session->checklist(2)
                     .value("chapters")
                     .toList()
                     .first()
                     .toMap()
                     .value("ticked")
                     .toList(),
                 (QVariantList { false }));

        const LoadResult result = ProjectStore::load(dir.path());
        QVERIFY(result.ok());
        QCOMPARE(ProjectStore::binderFile(result.project), readFile("binder.toml"));
        QCOMPARE(ProjectStore::projectFile(result.project), readFile("project.toml"));
        QVERIFY(session->setTick(1, "Check the POV", "f6g7h8j9k0", false));
        QVERIFY(!readFile("binder.toml").contains("[[tick]]"));
        QVERIFY(session->setPassChecks(1, { }));
        QVERIFY(session->checklist(1).value("checks").toStringList().isEmpty());
    }

    void readOnlyProjectRefusesEdits()
    {
        QFile file(dir.filePath("binder.toml"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QByteArray binder = file.readAll();
        file.close();
        binder.replace("format = 1", "format = 2");
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(binder);
        file.close();
        session->reload();
        QVERIFY(session->readOnly());
        QCOMPARE(session->add("scene"), QString());
        QCOMPARE(session->lastError(), QStringLiteral("This project cannot change."));
        QVERIFY(!session->rename("t6v4h9r2mz", "X"));
        QVERIFY(!session->moveToTrash("t6v4h9r2mz"));
    }
};

QTEST_GUILESS_MAIN(TestBinderEdit)
#include "tst_binderedit.moc"
