#include "DocxImport.h"
#include "ProjectStore.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <private/qzipwriter_p.h>

class TestDocxImport : public QObject
{
    Q_OBJECT

    static QStringList treeOf(const Project &project)
    {
        QStringList tree;
        for (const ProjectItem &item : project.items)
            tree.append(item.kind + ":" + item.title);
        return tree;
    }

    static QString textOf(const QString &folder, const ProjectItem &item)
    {
        QFile file(QDir(folder).filePath(item.file));
        return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
    }

private slots:
    // A DOCX that LibreOffice wrote from the sample: a title page, two
    // chapters as heading 1, and a scene break in the first chapter.
    void libreOfficeFileSplitsAtTheHeadings()
    {
        QTemporaryDir dir;
        const QString target = dir.filePath("lighthouse");
        const QList<LoadError> errors = DocxImport::importFile(
            QStringLiteral(FIXTURE_DIR "/docx/lighthouse.docx"), target, "Lighthouse");
        QVERIFY2(errors.isEmpty(),
                 qPrintable(errors.isEmpty() ? QString() : errors.first().text()));
        const LoadResult result = ProjectStore::load(target);
        QVERIFY2(result.ok(), qPrintable(result.ok() ? QString() : result.errors.first().text()));
        QCOMPARE(treeOf(result.project),
                 (QStringList { "chapter:Chapter 1: Dusk", "scene:Chapter 1: Dusk",
                                "scene:Chapter 1: Dusk, part 2", "chapter:Chapter 2: Morning",
                                "scene:Chapter 2: Morning", "scene:Chapter 2: Morning, part 2" }));
        const QString first = textOf(target, result.project.items.at(1));
        QVERIFY2(first.startsWith("The lamp had not gone out in forty years."), qPrintable(first));
        QVERIFY(first.contains("*Fair. Wind north-west. T. came by.*"));
        // The title page stays out.
        QVERIFY(!first.contains("The Lighthouse Keeper"));
    }

    void headingNamesCountInEveryLanguage()
    {
        QTemporaryDir dir;
        const QString docx = dir.filePath("de.docx");
        {
            QZipWriter zip(docx);
            zip.addFile("word/styles.xml",
                        "<w:styles "
                        "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
                        "<w:style w:type=\"paragraph\" w:styleId=\"berschrift1\"><w:name "
                        "w:val=\"heading 1\"/></w:style>"
                        "</w:styles>");
            zip.addFile(
                "word/document.xml",
                "<w:document "
                "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"><w:body>"
                "<w:p><w:r><w:t>Vorwort.</w:t></w:r></w:p>"
                "<w:p><w:pPr><w:pStyle "
                "w:val=\"berschrift1\"/></w:pPr><w:r><w:t>Eins</w:t></w:r></w:p>"
                "<w:p><w:r><w:rPr><w:b/></w:rPr><w:t>Fett</w:t></w:r><w:r><w:t "
                "xml:space=\"preserve\"> und </w:t></w:r>"
                "<w:r><w:rPr><w:i "
                "w:val=\"0\"/></w:rPr><w:t>gerade</w:t></w:r><w:r><w:br/><w:t>2*3</w:t></w:r></w:p>"
                "<w:p><w:r><w:t>* * *</w:t></w:r></w:p>"
                "<w:p><w:r><w:t>Danach.</w:t></w:r></w:p>"
                "</w:body></w:document>");
        }
        const QString target = dir.filePath("de");
        QVERIFY(DocxImport::importFile(docx, target, "De").isEmpty());
        const LoadResult result = ProjectStore::load(target);
        QVERIFY(result.ok());
        QCOMPARE(
            treeOf(result.project),
            (QStringList { "scene:Opening", "chapter:Eins", "scene:Eins", "scene:Eins, part 2" }));
        QCOMPARE(textOf(target, result.project.items.at(2)),
                 QStringLiteral("**Fett** und gerade\\\n2\\*3\n"));
        QCOMPARE(textOf(target, result.project.items.at(3)), QStringLiteral("Danach.\n"));
    }

    void notADocxIsAnError()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("x.docx"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not a zip");
        file.close();
        const QList<LoadError> errors
            = DocxImport::importFile(dir.filePath("x.docx"), dir.filePath("out"), "X");
        QCOMPARE(errors.size(), 1);
        QVERIFY(errors.first().message.contains("not a DOCX"));
        QVERIFY(!QDir(dir.filePath("out")).exists());
    }
};

QTEST_GUILESS_MAIN(TestDocxImport)
#include "tst_docximport.moc"
