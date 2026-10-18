#include "ProjectStore.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

namespace {

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return { };
    return file.readAll();
}

void writeFile(const QString &path, const QByteArray &bytes)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(bytes);
}

// Copies the fixture into dir, so a save cannot change the source tree.
void copyFixture(const QString &dir)
{
    const QString source = QStringLiteral(FIXTURE_DIR "/novel");
    QDirIterator it(source, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        writeFile(dir + path.mid(source.size()), readFile(path));
    }
}

LoadResult loadOk(const QString &folder)
{
    LoadResult result = ProjectStore::load(folder);
    QStringList texts;
    for (const LoadError &error : result.errors)
        texts.append(error.text());
    if (!result.ok())
        qWarning("%s", qPrintable(texts.join(u'\n')));
    return result;
}

// A binder with the keys in a mixed order, comments, unknown keys, and hard
// strings. The app must keep every value and write it in its own layout.
const char *messyBinder = R"(# A comment that the app does not keep.
format = 1
owner = "someone"

[[item]]
title = "Part"   # comment
section = "manuscript"
kind = "part"
id = "a1b2c3d4e5"
colour = "rust"

[[item]]
file = "manuscript/scene.md"
parent = "a1b2c3d4e5"
notes = """
He said "no" and left.
A tab	and a back\\slash.
It ends with a quote: \""""
synopsis = 'Literal \ string'
tags = [ "one",   "two" ]
kind = "scene"
id = "m1n2p3q4r5"
title = "Ünïcödé \"quoted\" title"
target = 1_500
rating = 4.5
seen = 2026-10-18
links = [{ id = "a1b2c3d4e5", note = "x" }]
"odd key" = true
empty = {}
mixed = [1, { a = 2 }, {}]
ctrl = "a\u0001b\rc\bd\fe\tf"
single = "line\nbreak\r"

[item.extra]
mood = "quiet"
)";

const char *simpleProject = "format = 1\n"
                            "title = \"Test\"\n"
                            "kind = \"novel\"\n"
                            "created = 2026-10-18T07:40:00Z\n";

} // namespace

class TestRoundTrip : public QObject
{
    Q_OBJECT

private slots:
    void fixtureIsInTheCanonicalLayout()
    {
        const LoadResult result = loadOk(QStringLiteral(FIXTURE_DIR "/novel"));
        QVERIFY(result.ok());
        QCOMPARE(ProjectStore::projectFile(result.project),
                 readFile(QStringLiteral(FIXTURE_DIR "/novel/project.toml")));
        QCOMPARE(ProjectStore::binderFile(result.project),
                 readFile(QStringLiteral(FIXTURE_DIR "/novel/binder.toml")));
    }

    void saveWithNoChangeWritesNothing()
    {
        QTemporaryDir dir;
        copyFixture(dir.path());
        const QString binder = dir.filePath(QStringLiteral("binder.toml"));
        const QDateTime before = QFileInfo(binder).lastModified();
        const QByteArray bytes = readFile(binder);

        const LoadResult result = loadOk(dir.path());
        QVERIFY(result.ok());
        QVERIFY(ProjectStore::save(result.project).isEmpty());
        QCOMPARE(readFile(binder), bytes);
        QCOMPARE(QFileInfo(binder).lastModified(), before);
    }

    void messyFileKeepsEveryValue()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("project.toml")), simpleProject);
        writeFile(dir.filePath(QStringLiteral("binder.toml")), messyBinder);
        writeFile(dir.filePath(QStringLiteral("manuscript/scene.md")), "Text.\n");

        const LoadResult first = loadOk(dir.path());
        QVERIFY(first.ok());
        QVERIFY(ProjectStore::save(first.project).isEmpty());
        const QByteArray saved = readFile(dir.filePath(QStringLiteral("binder.toml")));
        QVERIFY(!saved.contains("# A comment"));

        const LoadResult second = loadOk(dir.path());
        QVERIFY(second.ok());
        QCOMPARE(second.project.items.size(), first.project.items.size());
        QVERIFY(second.project.binderData == first.project.binderData);
        for (qsizetype i = 0; i < first.project.items.size(); ++i)
            QVERIFY2(second.project.items.at(i).data == first.project.items.at(i).data,
                     qPrintable(QStringLiteral("item %1 changed").arg(i)));

        // The second save gives the same bytes: the layout is stable.
        QCOMPARE(ProjectStore::binderFile(second.project), saved);
    }

    void canonicalLayout()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("project.toml")), simpleProject);
        writeFile(dir.filePath(QStringLiteral("binder.toml")), messyBinder);
        const LoadResult result = loadOk(dir.path());
        const QByteArray binder = ProjectStore::binderFile(result.project);
        // The known keys first in the order of the spec, then the others.
        QVERIFY(
            binder.startsWith("format = 1\nowner = \"someone\"\n\n[[item]]\nid = \"a1b2c3d4e5\"\n"
                              "kind = \"part\"\nsection = \"manuscript\"\ntitle = \"Part\"\n"
                              "colour = \"rust\"\n"));
        QVERIFY(binder.contains("tags = [\"one\", \"two\"]\ntarget = 1500\n"));
        QVERIFY(binder.contains("title = \"Ünïcödé \\\"quoted\\\" title\""));
        QVERIFY(binder.contains("notes = \"\"\"\nHe said \"no\" and left.\n"));
        QVERIFY(binder.contains("synopsis = \"Literal \\\\ string\""));
        QVERIFY(binder.contains("\n[item.extra]\nmood = \"quiet\"\n"));
        QVERIFY(binder.contains("\"odd key\" = true\n"));
        QVERIFY(binder.contains("\n[item.empty]\n"));
        QVERIFY(binder.contains("mixed = [1, { a = 2 }, {}]\n"));
        QVERIFY(binder.contains("ctrl = \"a\\u0001b\\rc\\bd\\fe\\tf\"\n"));
        QVERIFY(binder.endsWith("\n"));
    }

    void movedItemSavesInTreeOrder()
    {
        QTemporaryDir dir;
        copyFixture(dir.path());
        LoadResult result = loadOk(dir.path());
        QVERIFY(result.ok());
        Project &project = result.project;

        // Move the scene "The River Office" (3) from the chapter (2) to Part Two (5).
        project.items[2].children.removeOne(3);
        project.items[3].parent = 5;
        project.items[5].children.append(3);
        QVERIFY(ProjectStore::save(project).isEmpty());

        const LoadResult reloaded = loadOk(dir.path());
        QVERIFY(reloaded.ok());
        QStringList order;
        for (const ProjectItem &item : reloaded.project.items)
            order.append(item.title);
        QCOMPARE(order,
                 (QStringList { "Dedication", "Part One", "The Letter", "A Date She Did Not Know",
                                "Part Two", "The River Office", "The ledger", "Map of the river",
                                "Old opening" }));
        const ProjectItem &moved = reloaded.project.items.at(5);
        QCOMPARE(reloaded.project.items.at(moved.parent).title, QStringLiteral("Part Two"));
    }

    void readOnlyProjectIsNotSaved()
    {
        QTemporaryDir dir;
        copyFixture(dir.path());
        LoadResult result = loadOk(dir.path());
        result.project.readOnly = true;
        result.project.title = QStringLiteral("Changed");
        const QList<LoadError> errors = ProjectStore::save(result.project);
        QCOMPARE(errors.size(), 1);
        QVERIFY(errors.first().message.contains("newer"));
        QVERIFY(readFile(dir.filePath(QStringLiteral("project.toml"))).contains("The Salt Ledger"));
    }

    void failedSaveLeavesEachFileWhole()
    {
        QTemporaryDir dir;
        copyFixture(dir.path());
        LoadResult result = loadOk(dir.path());
        QVERIFY(result.ok());
        // A folder in place of binder.toml: no rename can replace it, even
        // for root.
        QVERIFY(QFile::remove(dir.filePath(QStringLiteral("binder.toml"))));
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("binder.toml")));
        result.project.title = QStringLiteral("A New Title");

        const QList<LoadError> errors = ProjectStore::save(result.project);
        QCOMPARE(errors.size(), 1);
        QCOMPARE(errors.first().file, QStringLiteral("binder.toml"));
        QVERIFY(QFileInfo(dir.filePath(QStringLiteral("binder.toml"))).isDir());
        // project.toml is whole: the new version, complete.
        QCOMPARE(readFile(dir.filePath(QStringLiteral("project.toml"))),
                 ProjectStore::projectFile(result.project));
        // No temporary file stays behind.
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files), (QStringList { "project.toml" }));
    }
};

QTEST_GUILESS_MAIN(TestRoundTrip)
#include "tst_roundtrip.moc"
