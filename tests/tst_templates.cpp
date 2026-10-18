#include "Naming.h"
#include "Templates.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

class TestTemplates : public QObject
{
    Q_OBJECT

private slots:
    void everyKindLoads_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<int>("textItems");
        QTest::newRow("short story") << "short-story" << 1;
        QTest::newRow("novella") << "novella" << 1;
        QTest::newRow("novel") << "novel" << 4;
        QTest::newRow("series") << "series" << 1;
        QTest::newRow("essay") << "essay" << 1;
        QTest::newRow("essay collection") << "essay-collection" << 2;
        QTest::newRow("non-fiction") << "non-fiction" << 5;
    }

    void everyKindLoads()
    {
        QFETCH(QString, kind);
        QFETCH(int, textItems);
        QTemporaryDir parent;
        const QString folder = parent.filePath(QStringLiteral("work"));

        const QList<LoadError> errors = Templates::create(
            folder, kind, QStringLiteral("The Salt Ledger"), QStringLiteral("A. Writer"));
        QVERIFY2(errors.isEmpty(),
                 qPrintable(errors.isEmpty() ? QString() : errors.first().text()));

        const LoadResult result = ProjectStore::load(folder);
        QVERIFY2(result.ok(), qPrintable(result.ok() ? QString() : result.errors.first().text()));
        const Project &project = result.project;
        QCOMPARE(project.kind, kind);
        QCOMPARE(project.title, QStringLiteral("The Salt Ledger"));
        QCOMPARE(project.data["author"].value_or(std::string()), std::string("A. Writer"));
        QVERIFY(project.data["created"].is_date_time());
        QVERIFY(!project.roots.isEmpty());

        int texts = 0;
        for (const ProjectItem &item : project.items) {
            if (item.file.isEmpty())
                continue;
            ++texts;
            QVERIFY2(!item.missingFile, qPrintable(item.file));
            QCOMPARE(QFileInfo(QDir(folder).filePath(item.file)).size(), 0);
        }
        QCOMPARE(texts, textItems);
        // A new project is already in the canonical layout.
        QVERIFY(ProjectStore::save(project).isEmpty());
    }

    void novelHasTheFullTree()
    {
        QTemporaryDir dir;
        QVERIFY(Templates::create(dir.path(), "novel", "N", QString()).isEmpty());
        const LoadResult result = ProjectStore::load(dir.path());
        QVERIFY(result.ok());
        QStringList tree;
        for (const ProjectItem &item : result.project.items)
            tree.append(item.kind + ":" + item.title + ":" + item.section);
        QCOMPARE(tree,
                 (QStringList { "page:Title page:front", "page:Dedication:front",
                                "part:Part One:manuscript", "chapter:Chapter 1:manuscript",
                                "scene:Scene 1:manuscript", "page:Acknowledgements:back" }));
        QCOMPARE(result.project.items.at(4).file, QStringLiteral("manuscript/scene-1.md"));
        QCOMPARE(result.project.data["goals"]["words"].value_or(0), 80000);
        QVERIFY(!result.project.data.contains("author"));
    }

    void refusesAFolderWithFiles()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath(QStringLiteral("keep.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        const QList<LoadError> errors = Templates::create(dir.path(), "novel", "N", QString());
        QCOMPARE(errors.size(), 1);
        QVERIFY(errors.first().message.contains("not empty"));
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files), QStringList { "keep.txt" });
    }

    void refusesAnUnknownKind()
    {
        QTemporaryDir dir;
        const QList<LoadError> errors
            = Templates::create(dir.filePath("x"), "poem", "N", QString());
        QCOMPARE(errors.size(), 1);
        QVERIFY(errors.first().message.contains("not a kind"));
        QVERIFY(!QFileInfo::exists(dir.filePath("x")));
    }

    void fileNames_data()
    {
        QTest::addColumn<QString>("title");
        QTest::addColumn<QStringList>("taken");
        QTest::addColumn<QString>("expected");
        QTest::newRow("plain") << "A Date She Did Not Know" << QStringList()
                               << "a-date-she-did-not-know.md";
        QTest::newRow("punctuation") << "  Hello, World!!  " << QStringList() << "hello-world.md";
        QTest::newRow("accents") << "Ünïcödé Café" << QStringList() << "ünïcödé-café.md";
        QTest::newRow("other script") << "Глава 1" << QStringList() << "глава-1.md";
        QTest::newRow("emoji dropped") << "Sea 🌊 Wall" << QStringList() << "sea-wall.md";
        QTest::newRow("empty") << "" << QStringList() << "untitled.md";
        QTest::newRow("only marks") << "!!! ???" << QStringList() << "untitled.md";
        QTest::newRow("taken") << "Scene" << QStringList { "scene.md" } << "scene-2.md";
        QTest::newRow("taken twice")
            << "Scene" << QStringList { "scene.md", "scene-2.md" } << "scene-3.md";
        QTest::newRow("long") << QString(70, u'a') << QStringList() << QString(60, u'a') + ".md";
        QTest::newRow("cut at a hyphen")
            << QString(59, u'a') + " b" << QStringList() << QString(59, u'a') + ".md";
    }

    void fileNames()
    {
        QFETCH(QString, title);
        QFETCH(QStringList, taken);
        QFETCH(QString, expected);
        QCOMPARE(Naming::fileNameFor(title, QSet<QString>(taken.begin(), taken.end())), expected);
    }

    void idsAreValidAndNew()
    {
        const QRegularExpression valid(QStringLiteral("^[0-9abcdefghjkmnpqrstvwxyz]{10}$"));
        QSet<QString> used;
        for (int i = 0; i < 500; ++i) {
            const QString id = Naming::newItemId(used);
            QVERIFY2(valid.match(id).hasMatch(), qPrintable(id));
            QVERIFY(!used.contains(id));
            used.insert(id);
        }
    }
};

QTEST_GUILESS_MAIN(TestTemplates)
#include "tst_templates.moc"
