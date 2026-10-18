#include "ProjectSession.h"
#include "ProjectStore.h"

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
