#include "Exporter.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <private/qzipreader_p.h>

class TestExporter : public QObject
{
    Q_OBJECT

    const QVariantList chapters {
        QVariantMap {
            { "title", "Dusk" },
            { "texts", QStringList { "The lamp *burned*.", "**Night** fell & stayed." } } },
        QVariantMap { { "title", "Morning" }, { "texts", QStringList { "Grey light." } } }
    };
    const QVariantMap book { { "title", "Lamp" },
                             { "author", "A. Writer" },
                             { "language", "en-GB" },
                             { "sceneBreak", "#" } };

private slots:
    void markdownKeepsTheMarksAndTheOrder()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("lamp.md");
        QCOMPARE(Exporter().write("markdown", chapters, book, QUrl::fromLocalFile(path)),
                 QString());
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(file.readAll()),
                 QStringLiteral("# Lamp\n\n*A. Writer*\n\n## Dusk\n\nThe lamp *burned*.\n\n\\#\n\n"
                                "**Night** fell & stayed.\n\n## Morning\n\nGrey light.\n\n"));
    }

    void docxHasHeadingsAndStyledRuns()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("lamp.docx");
        QCOMPARE(Exporter().write("docx", chapters, book, QUrl::fromLocalFile(path)), QString());
        QZipReader zip(path);
        QVERIFY(zip.isReadable());
        const QString document = QString::fromUtf8(zip.fileData("word/document.xml"));
        QCOMPARE(document.count("<w:pStyle w:val=\"Heading1\"/>"), 2);
        QVERIFY(document.contains("<w:rPr><w:i/></w:rPr><w:t xml:space=\"preserve\">burned</w:t>"));
        QVERIFY(document.contains("<w:rPr><w:b/></w:rPr><w:t xml:space=\"preserve\">Night</w:t>"));
        QVERIFY(document.contains("fell &amp; stayed."));
        QVERIFY(zip.fileData("word/styles.xml").contains("<w:pageBreakBefore/>"));
        QVERIFY(zip.fileData("docProps/core.xml").contains("<dc:creator>A. Writer</dc:creator>"));
    }

    void epubStartsWithAStoredMimetype()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("lamp.epub");
        QCOMPARE(Exporter().write("epub", chapters, book, QUrl::fromLocalFile(path)), QString());
        QZipReader zip(path);
        const QList<QZipReader::FileInfo> files = zip.fileInfoList();
        QCOMPARE(files.first().filePath, QStringLiteral("mimetype"));
        QCOMPARE(zip.fileData("mimetype"), QByteArray("application/epub+zip"));
        // A stored entry has its bytes as they are, right after its local header.
        QFile raw(path);
        QVERIFY(raw.open(QIODevice::ReadOnly));
        QVERIFY(raw.read(80).contains("mimetypeapplication/epub+zip"));
        const QString package = QString::fromUtf8(zip.fileData("OEBPS/content.opf"));
        QVERIFY(package.contains("<dc:language>en-GB</dc:language>"));
        QVERIFY(package.indexOf("chapter-1") < package.indexOf("chapter-2"));
        const QString first = QString::fromUtf8(zip.fileData("OEBPS/chapter-1.xhtml"));
        QVERIFY(first.contains("<p class=\"break\">#</p>"));
        QVERIFY(first.contains("<em>burned</em>"));
        QVERIFY(
            zip.fileData("OEBPS/nav.xhtml").contains("<a href=\"chapter-2.xhtml\">Morning</a>"));
    }

    void nothingToExportIsAnError()
    {
        QTemporaryDir dir;
        QVERIFY(!Exporter()
                     .write("epub", { }, book, QUrl::fromLocalFile(dir.filePath("x.epub")))
                     .isEmpty());
    }
};

QTEST_MAIN(TestExporter)
#include "tst_exporter.moc"
