#include "Exporter.h"

#include <QDateTime>
#include <QFile>
#include <QSaveFile>
#include <QTextBlock>
#include <QTextDocument>
#include <QUuid>
#include <private/qzipwriter_p.h>

namespace {

// A stretch of text with one style.
struct Run
{
    QString text;
    bool bold = false;
    bool italic = false;
};

// A paragraph of the book. A heading in a scene is a subheading of the
// chapter.
struct Paragraph
{
    enum Kind { Body, Subheading, Break };
    Kind kind = Body;
    QList<Run> runs;
};

struct Chapter
{
    QString title;
    QList<Paragraph> paragraphs;
};

QString text(const QVariantMap &map, const char *key)
{
    return map.value(QString::fromLatin1(key)).toString();
}

// The Markdown of each scene becomes paragraphs with runs, through the
// Markdown reader of Qt.
QList<Chapter> chaptersOf(const QVariantList &list)
{
    QList<Chapter> chapters;
    for (const QVariant &each : list) {
        const QVariantMap map = each.toMap();
        Chapter chapter { text(map, "title"), { } };
        const QStringList texts = map.value(QStringLiteral("texts")).toStringList();
        for (qsizetype i = 0; i < texts.size(); ++i) {
            if (i > 0)
                chapter.paragraphs.append(Paragraph { Paragraph::Break, { } });
            QTextDocument document;
            document.setMarkdown(texts.at(i));
            for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
                if (block.text().trimmed().isEmpty())
                    continue;
                Paragraph paragraph { block.blockFormat().headingLevel() > 0 ? Paragraph::Subheading
                                                                             : Paragraph::Body,
                                      { } };
                for (auto it = block.begin(); !it.atEnd(); ++it) {
                    const QTextFragment fragment = it.fragment();
                    const QTextCharFormat format = fragment.charFormat();
                    paragraph.runs.append(Run { fragment.text(),
                                                format.fontWeight() >= QFont::DemiBold,
                                                format.fontItalic() });
                }
                chapter.paragraphs.append(paragraph);
            }
        }
        chapters.append(chapter);
    }
    return chapters;
}

QString escaped(const QString &text)
{
    return text.toHtmlEscaped();
}

QString writeFile(const QString &path, const QByteArray &bytes)
{
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly))
        return out.errorString();
    out.write(bytes);
    return out.commit() ? QString() : out.errorString();
}

QString markdown(const QVariantList &chapters, const QVariantMap &book)
{
    QString out = QStringLiteral("# ") + text(book, "title") + QStringLiteral("\n\n");
    if (!text(book, "author").isEmpty())
        out += QStringLiteral("*") + text(book, "author") + QStringLiteral("*\n\n");
    const QString mark
        = text(book, "sceneBreak").isEmpty() ? QStringLiteral("***") : text(book, "sceneBreak");
    for (const QVariant &each : chapters) {
        const QVariantMap chapter = each.toMap();
        out += QStringLiteral("## ") + text(chapter, "title") + QStringLiteral("\n\n");
        QStringList scenes;
        for (const QString &scene : chapter.value(QStringLiteral("texts")).toStringList())
            scenes.append(scene.trimmed());
        // A "#" alone would be a heading in Markdown, so it is escaped.
        out += scenes.join(QStringLiteral("\n\n")
                           + (mark == QLatin1String("#") ? QStringLiteral("\\#") : mark)
                           + QStringLiteral("\n\n"))
            + QStringLiteral("\n\n");
    }
    return out;
}

// DOCX: the Office Open XML parts, the least that Word and LibreOffice read.
QString docxRuns(const QList<Run> &runs)
{
    QString out;
    for (const Run &run : runs) {
        out += QStringLiteral("<w:r>");
        if (run.bold || run.italic)
            out += QStringLiteral("<w:rPr>") + (run.bold ? QStringLiteral("<w:b/>") : QString())
                + (run.italic ? QStringLiteral("<w:i/>") : QString()) + QStringLiteral("</w:rPr>");
        out += QStringLiteral("<w:t xml:space=\"preserve\">") + escaped(run.text)
            + QStringLiteral("</w:t></w:r>");
    }
    return out;
}

QString docxParagraph(const QString &style, const QString &runs)
{
    return QStringLiteral("<w:p><w:pPr><w:pStyle w:val=\"%1\"/></w:pPr>%2</w:p>").arg(style, runs);
}

QString docx(const QString &path, const QList<Chapter> &chapters, const QVariantMap &book)
{
    const QString mark
        = text(book, "sceneBreak").isEmpty() ? QStringLiteral(" ") : text(book, "sceneBreak");
    QString body = docxParagraph(QStringLiteral("Title"), docxRuns({ { text(book, "title") } }));
    if (!text(book, "author").isEmpty())
        body += docxParagraph(QStringLiteral("Subtitle"), docxRuns({ { text(book, "author") } }));
    for (const Chapter &chapter : chapters) {
        body += docxParagraph(QStringLiteral("Heading1"), docxRuns({ { chapter.title } }));
        for (const Paragraph &paragraph : chapter.paragraphs) {
            if (paragraph.kind == Paragraph::Break)
                body += docxParagraph(QStringLiteral("SceneBreak"), docxRuns({ { mark } }));
            else
                body += docxParagraph(paragraph.kind == Paragraph::Subheading
                                          ? QStringLiteral("Heading2")
                                          : QStringLiteral("Normal"),
                                      docxRuns(paragraph.runs));
        }
    }
    const QString w
        = QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main");
    const QString document
        = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                         "<w:document xmlns:w=\"%1\"><w:body>%2<w:sectPr><w:pgMar w:top=\"1418\" "
                         "w:right=\"1418\" "
                         "w:bottom=\"1418\" w:left=\"1418\"/></w:sectPr></w:body></w:document>")
              .arg(w, body);
    const QString styles
        = QStringLiteral(
              "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
              "<w:styles xmlns:w=\"%1\">"
              "<w:style w:type=\"paragraph\" w:default=\"1\" w:styleId=\"Normal\"><w:name "
              "w:val=\"Normal\"/>"
              "<w:pPr><w:spacing w:after=\"120\" w:line=\"360\" w:lineRule=\"auto\"/></w:pPr>"
              "<w:rPr><w:sz w:val=\"24\"/></w:rPr></w:style>"
              "<w:style w:type=\"paragraph\" w:styleId=\"Title\"><w:name "
              "w:val=\"Title\"/><w:basedOn w:val=\"Normal\"/>"
              "<w:pPr><w:jc w:val=\"center\"/><w:spacing w:before=\"2400\" "
              "w:after=\"240\"/></w:pPr>"
              "<w:rPr><w:sz w:val=\"56\"/></w:rPr></w:style>"
              "<w:style w:type=\"paragraph\" w:styleId=\"Subtitle\"><w:name "
              "w:val=\"Subtitle\"/><w:basedOn w:val=\"Normal\"/>"
              "<w:pPr><w:jc w:val=\"center\"/></w:pPr><w:rPr><w:i/><w:sz "
              "w:val=\"28\"/></w:rPr></w:style>"
              "<w:style w:type=\"paragraph\" w:styleId=\"Heading1\"><w:name w:val=\"heading "
              "1\"/><w:basedOn w:val=\"Normal\"/>"
              "<w:next w:val=\"Normal\"/><w:pPr><w:pageBreakBefore/><w:jc w:val=\"center\"/>"
              "<w:spacing w:before=\"720\" w:after=\"480\"/><w:outlineLvl w:val=\"0\"/></w:pPr>"
              "<w:rPr><w:b/><w:sz w:val=\"36\"/></w:rPr></w:style>"
              "<w:style w:type=\"paragraph\" w:styleId=\"Heading2\"><w:name w:val=\"heading "
              "2\"/><w:basedOn w:val=\"Normal\"/>"
              "<w:next w:val=\"Normal\"/><w:pPr><w:outlineLvl w:val=\"1\"/></w:pPr>"
              "<w:rPr><w:b/><w:sz w:val=\"28\"/></w:rPr></w:style>"
              "<w:style w:type=\"paragraph\" w:styleId=\"SceneBreak\"><w:name w:val=\"Scene "
              "Break\"/><w:basedOn w:val=\"Normal\"/>"
              "<w:pPr><w:jc w:val=\"center\"/><w:spacing w:before=\"240\" "
              "w:after=\"240\"/></w:pPr></w:style>"
              "</w:styles>")
              .arg(w);
    const QString core
        = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                         "<cp:coreProperties "
                         "xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/"
                         "core-properties\" "
                         "xmlns:dc=\"http://purl.org/dc/elements/1.1/\"><dc:title>%1</"
                         "dc:title><dc:creator>%2</dc:creator>"
                         "<dc:language>%3</dc:language></cp:coreProperties>")
              .arg(escaped(text(book, "title")), escaped(text(book, "author")),
                   escaped(text(book, "language")));

    QZipWriter zip(path);
    if (zip.status() != QZipWriter::NoError)
        return QStringLiteral("Cannot write %1.").arg(path);
    zip.addFile(QStringLiteral("[Content_Types].xml"),
                QByteArrayLiteral(
                    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                    "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
                    "<Default Extension=\"rels\" "
                    "ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
                    "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
                    "<Override PartName=\"/word/document.xml\" "
                    "ContentType=\"application/"
                    "vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
                    "<Override PartName=\"/word/styles.xml\" "
                    "ContentType=\"application/"
                    "vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/>"
                    "<Override PartName=\"/docProps/core.xml\" "
                    "ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>"
                    "</Types>"));
    zip.addFile(
        QStringLiteral("_rels/.rels"),
        QByteArrayLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            "<Relationship Id=\"rId1\" "
            "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
            "officeDocument\" Target=\"word/document.xml\"/>"
            "<Relationship Id=\"rId2\" "
            "Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/"
            "core-properties\" Target=\"docProps/core.xml\"/>"
            "</Relationships>"));
    zip.addFile(
        QStringLiteral("word/_rels/document.xml.rels"),
        QByteArrayLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            "<Relationship Id=\"rId1\" "
            "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" "
            "Target=\"styles.xml\"/>"
            "</Relationships>"));
    zip.addFile(QStringLiteral("word/document.xml"), document.toUtf8());
    zip.addFile(QStringLiteral("word/styles.xml"), styles.toUtf8());
    zip.addFile(QStringLiteral("docProps/core.xml"), core.toUtf8());
    zip.close();
    return zip.status() == QZipWriter::NoError ? QString()
                                               : QStringLiteral("Cannot write %1.").arg(path);
}

// EPUB 3: one XHTML file for each chapter, a navigation file, and the package.
QString xhtmlRuns(const QList<Run> &runs)
{
    QString out;
    for (const Run &run : runs) {
        QString part = escaped(run.text);
        if (run.italic)
            part = QStringLiteral("<em>") + part + QStringLiteral("</em>");
        if (run.bold)
            part = QStringLiteral("<strong>") + part + QStringLiteral("</strong>");
        out += part;
    }
    return out;
}

QString xhtmlPage(const QString &language, const QString &title, const QString &body)
{
    return QStringLiteral(
               "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE html>\n"
               "<html xmlns=\"http://www.w3.org/1999/xhtml\" "
               "xmlns:epub=\"http://www.idpf.org/2007/ops\" "
               "lang=\"%1\" xml:lang=\"%1\"><head><meta charset=\"UTF-8\"/><title>%2</title>"
               "<link rel=\"stylesheet\" type=\"text/css\" "
               "href=\"style.css\"/></head><body>%3</body></html>\n")
        .arg(language, escaped(title), body);
}

QString epub(const QString &path, const QList<Chapter> &chapters, const QVariantMap &book)
{
    const QString language
        = text(book, "language").isEmpty() ? QStringLiteral("en") : text(book, "language");
    const QString title = text(book, "title");
    const QString mark = text(book, "sceneBreak");
    QZipWriter zip(path);
    if (zip.status() != QZipWriter::NoError)
        return QStringLiteral("Cannot write %1.").arg(path);
    // The mimetype comes first and stays uncompressed, as EPUB requires.
    zip.setCompressionPolicy(QZipWriter::NeverCompress);
    zip.addFile(QStringLiteral("mimetype"), QByteArrayLiteral("application/epub+zip"));
    zip.setCompressionPolicy(QZipWriter::AlwaysCompress);
    zip.addFile(
        QStringLiteral("META-INF/container.xml"),
        QByteArrayLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">"
            "<rootfiles><rootfile full-path=\"OEBPS/content.opf\" "
            "media-type=\"application/oebps-package+xml\"/>"
            "</rootfiles></container>\n"));
    zip.addFile(QStringLiteral("OEBPS/style.css"),
                QByteArrayLiteral(
                    "body { margin: 0 5%; line-height: 1.5; }\n"
                    "h1 { text-align: center; margin: 3em 0 1.5em; }\n"
                    "p { margin: 0; text-indent: 1.5em; }\n"
                    "h1 + p, h2 + p, p.break + p { text-indent: 0; }\n"
                    "p.break { text-align: center; text-indent: 0; margin: 1em 0; }\n"
                    "p.author { text-align: center; text-indent: 0; font-style: italic; }\n"));

    QString manifest;
    QString spine;
    QString toc;
    QString titlePage = QStringLiteral("<h1>") + escaped(title) + QStringLiteral("</h1>");
    if (!text(book, "author").isEmpty())
        titlePage += QStringLiteral("<p class=\"author\">") + escaped(text(book, "author"))
            + QStringLiteral("</p>");
    zip.addFile(QStringLiteral("OEBPS/title.xhtml"),
                xhtmlPage(language, title, titlePage).toUtf8());
    manifest += QStringLiteral(
        "<item id=\"title\" href=\"title.xhtml\" media-type=\"application/xhtml+xml\"/>");
    spine += QStringLiteral("<itemref idref=\"title\"/>");
    for (qsizetype i = 0; i < chapters.size(); ++i) {
        const Chapter &chapter = chapters.at(i);
        QString body = QStringLiteral("<h1>") + escaped(chapter.title) + QStringLiteral("</h1>");
        for (const Paragraph &paragraph : chapter.paragraphs) {
            if (paragraph.kind == Paragraph::Break)
                body += QStringLiteral("<p class=\"break\">")
                    + (mark.isEmpty() ? QStringLiteral("&#160;") : escaped(mark))
                    + QStringLiteral("</p>");
            else if (paragraph.kind == Paragraph::Subheading)
                body
                    += QStringLiteral("<h2>") + xhtmlRuns(paragraph.runs) + QStringLiteral("</h2>");
            else
                body += QStringLiteral("<p>") + xhtmlRuns(paragraph.runs) + QStringLiteral("</p>");
        }
        const QString name = QStringLiteral("chapter-%1").arg(i + 1);
        zip.addFile(QStringLiteral("OEBPS/") + name + QStringLiteral(".xhtml"),
                    xhtmlPage(language, chapter.title, body).toUtf8());
        manifest += QStringLiteral(
                        "<item id=\"%1\" href=\"%1.xhtml\" media-type=\"application/xhtml+xml\"/>")
                        .arg(name);
        spine += QStringLiteral("<itemref idref=\"%1\"/>").arg(name);
        toc += QStringLiteral("<li><a href=\"%1.xhtml\">%2</a></li>")
                   .arg(name, escaped(chapter.title));
    }
    zip.addFile(QStringLiteral("OEBPS/nav.xhtml"),
                xhtmlPage(language, title,
                          QStringLiteral("<nav epub:type=\"toc\" id=\"toc\"><h1>Contents</h1><ol>")
                              + toc + QStringLiteral("</ol></nav>"))
                    .toUtf8());
    const QString modified
        = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ssZ"));
    const QString package
        = QStringLiteral(
              "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
              "<package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\" "
              "unique-identifier=\"id\">"
              "<metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
              "<dc:identifier id=\"id\">urn:uuid:%1</dc:identifier><dc:title>%2</dc:title>"
              "<dc:language>%3</dc:language>%4<meta "
              "property=\"dcterms:modified\">%5</meta></metadata>"
              "<manifest><item id=\"nav\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" "
              "properties=\"nav\"/>"
              "<item id=\"css\" href=\"style.css\" media-type=\"text/css\"/>%6</manifest>"
              "<spine>%7</spine></package>\n")
              .arg(QUuid::createUuid().toString(QUuid::WithoutBraces), escaped(title), language,
                   text(book, "author").isEmpty() ? QString()
                                                  : QStringLiteral("<dc:creator>")
                           + escaped(text(book, "author")) + QStringLiteral("</dc:creator>"),
                   modified, manifest, spine);
    zip.addFile(QStringLiteral("OEBPS/content.opf"), package.toUtf8());
    zip.close();
    return zip.status() == QZipWriter::NoError ? QString()
                                               : QStringLiteral("Cannot write %1.").arg(path);
}

} // namespace

QString Exporter::write(const QString &format, const QVariantList &chapters,
                        const QVariantMap &book, const QUrl &file) const
{
    if (chapters.isEmpty())
        return QStringLiteral("Nothing to export: no scene is in the manuscript.");
    const QString path = file.toLocalFile();
    if (format == QLatin1String("markdown"))
        return writeFile(path, markdown(chapters, book).toUtf8());
    // A zip is written in place, so an old file goes first.
    QFile::remove(path);
    if (format == QLatin1String("docx"))
        return docx(path, chaptersOf(chapters), book);
    if (format == QLatin1String("epub"))
        return epub(path, chaptersOf(chapters), book);
    return QStringLiteral("Unknown format: %1").arg(format);
}
