#include "MarkdownImport.h"
#include "ProjectStore.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class TestMarkdownImport : public QObject
{
    Q_OBJECT

    static void write(const QString &path, const QByteArray &text)
    {
        QDir().mkpath(QFileInfo(path).path());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(text);
    }

private slots:
    void filesBecomeScenesInNaturalOrder()
    {
        QTemporaryDir dir;
        const QString source = dir.filePath("drafts");
        write(source + "/10-the-end.md", "It ended.\n");
        write(source + "/2-middle.md", "# The Middle\n\nIt went on.\r\n");
        write(source + "/1-start.md", "It began.\n");
        write(source + "/notes.txt", "Not Markdown.\n");
        write(source + "/Book Two/b.md", "Second b.\n");
        write(source + "/Book Two/a.md", "Second a.\n");
        QDir(source).mkpath("Empty");

        const QString target = dir.filePath("book");
        const QList<LoadError> errors = MarkdownImport::importFolder(source, target, "Book");
        QVERIFY2(errors.isEmpty(),
                 qPrintable(errors.isEmpty() ? QString() : errors.first().text()));
        const LoadResult result = ProjectStore::load(target);
        QVERIFY2(result.ok(), qPrintable(result.ok() ? QString() : result.errors.first().text()));
        QStringList tree;
        for (const ProjectItem &item : result.project.items)
            tree.append(item.kind + ":" + item.title);
        QCOMPARE(tree,
                 (QStringList { "scene:1 start", "scene:The Middle", "scene:10 the end",
                                "chapter:Book Two", "scene:a", "scene:b" }));
        // The heading became the title and left the text.
        QFile middle(QDir(target).filePath(result.project.items.at(1).file));
        QVERIFY(middle.open(QIODevice::ReadOnly));
        QCOMPARE(middle.readAll(), QByteArray("It went on.\n"));
        QVERIFY(QFile::exists(source + "/2-middle.md"));
    }

    void folderWithNoMarkdownIsAnError()
    {
        QTemporaryDir dir;
        write(dir.filePath("src/readme.txt"), "x");
        const QList<LoadError> errors
            = MarkdownImport::importFolder(dir.filePath("src"), dir.filePath("out"), "X");
        QCOMPARE(errors.size(), 1);
        QVERIFY(errors.first().message.contains("no Markdown files"));
        QVERIFY(!QDir(dir.filePath("out")).exists());
        QVERIFY(!MarkdownImport::importFolder(dir.filePath("none"), dir.filePath("out"), "X")
                     .isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestMarkdownImport)
#include "tst_markdownimport.moc"
