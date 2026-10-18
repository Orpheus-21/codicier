#include "ProjectStore.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

QString projectText()
{
    return QStringLiteral("format = 1\n"
                          "title = \"Test\"\n"
                          "kind = \"novel\"\n"
                          "created = 2026-10-18T07:40:00Z\n");
}

// A good binder: one part, one chapter, one scene.
QString goodBinder()
{
    return QStringLiteral("format = 1\n"
                          "\n"
                          "[[item]]\n"
                          "id = \"a1b2c3d4e5\"\n"
                          "kind = \"part\"\n"
                          "section = \"manuscript\"\n"
                          "title = \"Part\"\n"
                          "\n"
                          "[[item]]\n"
                          "id = \"f6g7h8j9k0\"\n"
                          "kind = \"chapter\"\n"
                          "parent = \"a1b2c3d4e5\"\n"
                          "title = \"Chapter\"\n"
                          "\n"
                          "[[item]]\n"
                          "id = \"m1n2p3q4r5\"\n"
                          "kind = \"scene\"\n"
                          "parent = \"f6g7h8j9k0\"\n"
                          "title = \"Scene\"\n"
                          "file = \"manuscript/scene.md\"\n");
}

void write(const QString &path, const QString &text)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(text.toUtf8());
}

// Writes a project with the given files into dir and loads it.
LoadResult loadWith(const QTemporaryDir &dir, const QString &project, const QString &binder)
{
    QDir(dir.path()).mkpath(QStringLiteral("manuscript"));
    write(dir.filePath(QStringLiteral("project.toml")), project);
    write(dir.filePath(QStringLiteral("binder.toml")), binder);
    write(dir.filePath(QStringLiteral("manuscript/scene.md")), QStringLiteral("Text.\n"));
    return ProjectStore::load(dir.path());
}

QStringList texts(const LoadResult &result)
{
    QStringList list;
    for (const LoadError &error : result.errors)
        list.append(error.text());
    return list;
}

} // namespace

class TestProjectStore : public QObject
{
    Q_OBJECT

private slots:
    void loadsTheFixture()
    {
        const LoadResult result = ProjectStore::load(QStringLiteral(FIXTURE_DIR "/novel"));
        QVERIFY2(result.ok(), qPrintable(texts(result).join(u'\n')));
        const Project &project = result.project;
        QCOMPARE(project.title, QStringLiteral("The Salt Ledger"));
        QCOMPARE(project.kind, QStringLiteral("novel"));
        QVERIFY(!project.readOnly);
        QCOMPARE(project.items.size(), 9);

        QStringList roots;
        for (const int i : project.roots)
            roots.append(project.items.at(i).title);
        QCOMPARE(roots,
                 (QStringList { "Dedication", "Part One", "Part Two", "The ledger",
                                "Map of the river", "Old opening" }));

        const ProjectItem &chapter = project.items.at(2);
        QCOMPARE(chapter.title, QStringLiteral("The Letter"));
        QCOMPARE(chapter.parent, 1);
        QCOMPARE(chapter.children, (QList<int> { 3, 4 }));
        QCOMPARE(chapter.section, QStringLiteral("manuscript"));

        const ProjectItem &scene = project.items.at(4);
        QCOMPARE(scene.id, QStringLiteral("t6v4h9r2mz"));
        QCOMPARE(scene.file, QStringLiteral("manuscript/a-date-she-did-not-know.md"));
        QCOMPARE(scene.section, QStringLiteral("manuscript"));
        QVERIFY(!scene.missingFile);
        QCOMPARE(scene.data["status"].value_or(std::string()), std::string("revised"));
        QCOMPARE(scene.data["tags"].as_array()->size(), std::size_t(3));
        // A key that the app does not know stays in the data.
        QCOMPARE(scene.data["mood"].value_or(std::string()), std::string("quiet"));

        QCOMPARE(project.items.at(8).section, QStringLiteral("trash"));
        QCOMPARE(project.data["goals"]["words"].value_or(0), 90000);
    }

    void missingFileKeepsTheItem()
    {
        QTemporaryDir dir;
        const LoadResult result
            = loadWith(dir, projectText(), QString(goodBinder()).replace("scene.md", "gone.md"));
        QVERIFY2(result.ok(), qPrintable(texts(result).join(u'\n')));
        QVERIFY(result.project.items.at(2).missingFile);
    }

    void newerFormatIsReadOnly()
    {
        QTemporaryDir dir;
        const LoadResult result = loadWith(
            dir, QString(projectText()).replace("format = 1", "format = 2"), goodBinder());
        QVERIFY2(result.ok(), qPrintable(texts(result).join(u'\n')));
        QVERIFY(result.project.readOnly);
    }

    void missingFolderIsAnError()
    {
        const LoadResult result = ProjectStore::load(QStringLiteral("/no/such/project"));
        QCOMPARE(result.errors.size(), 1);
        QVERIFY(result.errors.first().message.contains("does not exist"));
    }

    void badFilesGiveErrors_data()
    {
        QTest::addColumn<QString>("project");
        QTest::addColumn<QString>("binder");
        QTest::addColumn<QString>("expected");

        const auto bad = [](const char *from, const char *to) {
            return QString(goodBinder()).replace(QString::fromUtf8(from), QString::fromUtf8(to));
        };
        QTest::newRow("toml syntax")
            << projectText() << bad("title = \"Chapter\"", "title = \"Chapter")
            << "binder.toml:13:";
        QTest::newRow("no binder") << projectText() << QString() << "binder.toml:";
        QTest::newRow("no format")
            << projectText() << bad("format = 1\n", "") << "binder.toml: `format` is missing";
        QTest::newRow("format 0") << projectText() << bad("format = 1", "format = 0")
                                  << "binder.toml:1: `format` must be 1 or more";
        QTest::newRow("item not a table")
            << projectText() << QStringLiteral("format = 1\nitem = 3\n")
            << "binder.toml:2: `item` must be a list";
        QTest::newRow("bad id") << projectText() << bad("m1n2p3q4r5", "m1n2p3q4r")
                                << "binder.toml:16: `id` \"m1n2p3q4r\" must have 10 characters";
        QTest::newRow("id with i")
            << projectText() << bad("m1n2p3q4r5", "i1n2p3q4r5") << "must have 10 characters";
        QTest::newRow("same id twice") << projectText() << bad("m1n2p3q4r5", "f6g7h8j9k0")
                                       << "binder.toml:15: the id \"f6g7h8j9k0\" is used twice";
        QTest::newRow("bad kind") << projectText() << bad("kind = \"scene\"", "kind = \"poem\"")
                                  << "binder.toml:17: `kind` \"poem\" is not a kind of item";
        QTest::newRow("no title") << projectText() << bad("title = \"Scene\"\n", "")
                                  << "binder.toml:15: `title` is missing";
        QTest::newRow("title not a string")
            << projectText() << bad("title = \"Scene\"", "title = 4")
            << "binder.toml:19: `title` must be a string";
        QTest::newRow("top level without section")
            << projectText() << bad("section = \"manuscript\"\n", "")
            << "binder.toml:3: an item at the top level needs a `section`";
        QTest::newRow("unknown section") << projectText() << bad("\"manuscript\"\n", "\"attic\"\n")
                                         << "`section` \"attic\" is not a section";
        QTest::newRow("unknown parent")
            << projectText() << bad("parent = \"f6g7h8j9k0\"", "parent = \"zzzzzzzzzz\"")
            << "binder.toml:15: the parent \"zzzzzzzzzz\" does not exist";
        QTest::newRow("scene holds items") << projectText()
                                           << goodBinder()
                + "\n[[item]]\nid = \"q1q2q3q4q5\"\nkind = \"note\"\nparent = "
                  "\"m1n2p3q4r5\"\ntitle = \"N\"\nfile = \"notes/n.md\"\n"
                                           << "binder.toml:22: a scene cannot hold other items";
        QTest::newRow("loop") << projectText()
                              << bad("section = \"manuscript\"", "parent = \"f6g7h8j9k0\"")
                              << "form a loop";
        QTest::newRow("scene in notes")
            << projectText() << bad("section = \"manuscript\"", "section = \"notes\"")
            << "a part cannot be in the notes section";
        QTest::newRow("scene without file")
            << projectText() << bad("file = \"manuscript/scene.md\"\n", "")
            << "binder.toml:15: a scene needs a `file`";
        QTest::newRow("file outside")
            << projectText() << bad("manuscript/scene.md", "manuscript/../../etc/passwd")
            << "the file of a scene must be in manuscript/";
        QTest::newRow("file in wrong folder")
            << projectText() << bad("manuscript/scene.md", "notes/scene.md")
            << "the file of a scene must be in manuscript/";
        QTest::newRow("part with file")
            << projectText()
            << bad("title = \"Part\"", "title = \"Part\"\nfile = \"manuscript/p.md\"")
            << "a part has no `file`";
        QTest::newRow("bad status") << projectText() << goodBinder() + "status = \"done\"\n"
                                    << "binder.toml:21: `status` must be draft, revised, or final";
        QTest::newRow("bad tags") << projectText() << goodBinder() + "tags = [\"a\", 2]\n"
                                  << "binder.toml:21: `tags` must hold only strings";
        QTest::newRow("bad target") << projectText() << goodBinder() + "target = \"many\"\n"
                                    << "binder.toml:21: `target` must be an integer";

        QTest::newRow("project kind")
            << QString(projectText()).replace("\"novel\"", "\"poem\"") << goodBinder()
            << "project.toml:3: `kind` \"poem\" is not a kind of project";
        QTest::newRow("project no title")
            << QString(projectText()).replace("title = \"Test\"\n", "") << goodBinder()
            << "project.toml: `title` is missing";
        QTest::newRow("project no created")
            << QString(projectText()).replace("created = 2026-10-18T07:40:00Z\n", "")
            << goodBinder() << "project.toml: `created` is missing";
        QTest::newRow("project created is a date")
            << QString(projectText()).replace("2026-10-18T07:40:00Z", "2026-10-18") << goodBinder()
            << "project.toml:4: `created` must be a date and time";
        QTest::newRow("project goals words")
            << projectText() + "[goals]\nwords = \"many\"\n"
            << goodBinder() << "project.toml:6: `words` must be an integer";
        QTest::newRow("project author")
            << projectText() + "author = 7\n"
            << goodBinder() << "project.toml:5: `author` must be a string";
    }

    void badFilesGiveErrors()
    {
        QFETCH(QString, project);
        QFETCH(QString, binder);
        QFETCH(QString, expected);
        QTemporaryDir dir;
        QDir(dir.path()).mkpath(QStringLiteral("notes"));
        LoadResult result;
        if (binder.isEmpty()) {
            write(dir.filePath(QStringLiteral("project.toml")), project);
            result = ProjectStore::load(dir.path());
        } else {
            result = loadWith(dir, project, binder);
        }
        QVERIFY(!result.ok());
        const QString all = texts(result).join(u'\n');
        QVERIFY2(all.contains(expected),
                 qPrintable(QStringLiteral("wanted \"%1\" in:\n%2").arg(expected, all)));
    }
};

QTEST_GUILESS_MAIN(TestProjectStore)
#include "tst_projectstore.moc"
