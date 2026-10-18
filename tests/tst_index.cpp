#include "Index.h"
#include "ProjectStore.h"
#include "Templates.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

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

QStringList build(const Project &project)
{
    Index index(project.folder);
    const QString opened = index.open();
    if (!opened.isEmpty())
        return { "open failed: " + opened };
    const QString built = index.rebuild(project);
    if (!built.isEmpty())
        return { "rebuild failed: " + built };
    return index.dump();
}

} // namespace

class TestIndex : public QObject
{
    Q_OBJECT

private slots:
    void rebuildGivesTheSameRows()
    {
        QTemporaryDir dir;
        copyFixture(dir.path());
        const LoadResult result = ProjectStore::load(dir.path());
        QVERIFY(result.ok());

        const QStringList first = build(result.project);
        QVERIFY2(first.size() == 9, qPrintable(first.join(u'\n')));
        QVERIFY(QFile::remove(Index::path(dir.path())));
        const QStringList second = build(result.project);
        QCOMPARE(second, first);
    }

    void rowsHoldTheItemAndItsText()
    {
        QTemporaryDir dir;
        copyFixture(dir.path());
        const LoadResult result = ProjectStore::load(dir.path());
        const QStringList rows = build(result.project);
        QVERIFY(
            rows.contains("t6v4h9r2mz|scene|A Date She Did Not Know|manuscript|"
                          "manuscript/a-date-she-did-not-know.md|18|The letter said *the fourth "
                          "of March*, and nothing else on the first line.[^1]\n\n"
                          "[^1]: The year is missing.\n"));
        // A research file has no text in the index.
        QVERIFY(
            rows.contains("y0z1a2b3c4|file|Map of the river|research|research/river-map.txt|0|"));
    }

    void folderIsIgnoredByGit()
    {
        QTemporaryDir dir;
        Index index(dir.path());
        QVERIFY(index.open().isEmpty());
        QFile ignore(dir.filePath(".codicier/.gitignore"));
        QVERIFY(ignore.open(QIODevice::ReadOnly));
        QCOMPARE(ignore.readAll(), QByteArray("*\n"));
    }

    void searchFindsTextAndTitles()
    {
        QTemporaryDir dir;
        copyFixture(dir.path());
        const LoadResult result = ProjectStore::load(dir.path());
        Index index(dir.path());
        QVERIFY(index.open().isEmpty());
        QVERIFY(index.rebuild(result.project).isEmpty());

        const QList<SearchHit> text = index.search("fourth march");
        QCOMPARE(text.size(), 1);
        QCOMPARE(text.first().id, QStringLiteral("t6v4h9r2mz"));
        QVERIFY2(text.first().snippet.contains("[fourth]"), qPrintable(text.first().snippet));

        const QList<SearchHit> title = index.search("river");
        QStringList ids;
        for (const SearchHit &hit : title)
            ids.append(hit.id);
        QVERIFY(ids.contains("m1n2p3q4r5"));
        QVERIFY(ids.contains("y0z1a2b3c4"));

        QVERIFY(index.search("nowhere").isEmpty());
        QVERIFY(index.search("   ").isEmpty());
        // FTS5 operators in a query are plain words, not syntax.
        QVERIFY(index.search("\"unclosed AND NOT (").isEmpty());
    }

    void searchIgnoresAccents()
    {
        QTemporaryDir parent;
        const QString folder = parent.filePath("essay");
        QVERIFY(Templates::create(folder, "essay", "Notes", QString()).isEmpty());
        LoadResult result = ProjectStore::load(folder);
        QFile file(QDir(folder).filePath(result.project.items.first().file));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("She drank a café crème by the canal.\n");
        file.close();

        Index index(folder);
        QVERIFY(index.open().isEmpty());
        QVERIFY(index.rebuild(result.project).isEmpty());
        QCOMPARE(index.search("cafe creme").size(), 1);
        QCOMPARE(index.search("CAFÉ").size(), 1);
    }

    void updateReadsOneItem()
    {
        QTemporaryDir parent;
        const QString folder = parent.filePath("essay");
        QVERIFY(Templates::create(folder, "essay", "Notes", QString()).isEmpty());
        LoadResult result = ProjectStore::load(folder);
        const ProjectItem item = result.project.items.first();
        Index index(folder);
        QVERIFY(index.update(result.project, item.id) == QStringLiteral("the index is not open"));
        QVERIFY(index.open().isEmpty());
        QVERIFY(index.rebuild(result.project).isEmpty());
        QVERIFY(index.search("lighthouse").isEmpty());

        QFile file(QDir(folder).filePath(item.file));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("The lighthouse keeper slept.\n");
        file.close();
        QVERIFY(index.update(result.project, item.id).isEmpty());
        const QList<SearchHit> hits = index.search("lighthouse", "<", ">");
        QCOMPARE(hits.size(), 1);
        QVERIFY2(hits.first().snippet.contains("<lighthouse>"), qPrintable(hits.first().snippet));
        // One row for each item, also after an update.
        QCOMPARE(index.dump().size(), result.project.items.size());

        // An item that left the project leaves the index.
        result.project.items.removeFirst();
        QVERIFY(index.update(result.project, item.id).isEmpty());
        QVERIFY(index.search("lighthouse").isEmpty());
    }

    void firstMatchFindsWholeWords()
    {
        QCOMPARE(Index::firstMatch("Start the art class.", "art"), 10);
        QCOMPARE(Index::firstMatch("A Café, then a cafe.", "CAFE other"), 2);
        QCOMPARE(Index::firstMatch("Nothing here.", "art"), -1);
        QCOMPARE(Index::firstMatch("Any text.", "  "), -1);
        QCOMPARE(Index::firstMatch("art", "art"), 0);
    }

    void oldSchemaIsBuiltAgain()
    {
        QTemporaryDir dir;
        copyFixture(dir.path());
        const LoadResult result = ProjectStore::load(dir.path());
        QVERIFY(!build(result.project).isEmpty());
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "old");
            db.setDatabaseName(Index::path(dir.path()));
            QVERIFY(db.open());
            QSqlQuery query(db);
            QVERIFY(query.exec("PRAGMA user_version = 0"));
            QVERIFY(query.exec("DROP TABLE text"));
            db.close();
        }
        QSqlDatabase::removeDatabase("old");
        const QStringList again = build(result.project);
        QVERIFY2(again.size() == 9, qPrintable(again.join(u'\n')));
    }

    void countsWords_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<int>("words");
        QTest::newRow("empty") << "" << 0;
        QTest::newRow("plain") << "The river went on." << 4;
        QTest::newRow("apostrophe") << "She didn't know the river’s name." << 6;
        QTest::newRow("marks") << "*the fourth* of **March**" << 4;
        QTest::newRow("dashes and numbers") << "1998 to 2001, 12-14 May" << 6;
        QTest::newRow("other script") << "Глава один" << 2;
        QTest::newRow("footnote marks") << "line.[^1] More.[^note-2]\n\n[^1]: The year." << 4;
        QTest::newRow("open bracket") << "a [^ b" << 2;
    }

    void countsWords()
    {
        QFETCH(QString, text);
        QFETCH(int, words);
        QCOMPARE(Index::countWords(text), words);
    }
};

QTEST_GUILESS_MAIN(TestIndex)
#include "tst_index.moc"
