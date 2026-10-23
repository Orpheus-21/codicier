#include "DocxImport.h"

#include "MarkdownImport.h"

#include <QHash>
#include <QXmlStreamReader>
#include <private/qzipreader_p.h>

namespace {

// A style of the document: its outline level (0 for heading 1), or -1, and
// whether it is a title of the whole document.
struct Style
{
    int level = -1;
    bool title = false;
};

QString attribute(const QXmlStreamReader &xml, const char *name)
{
    static constexpr QLatin1StringView wordNs(
        "http://schemas.openxmlformats.org/wordprocessingml/2006/main");
    return xml.attributes().value(wordNs, QLatin1String(name)).toString();
}

// The styles by id. A style is a heading by its outline level or by its
// name, "heading 1" to "heading 9", which Word keeps in English in every
// language.
QHash<QString, Style> readStyles(const QByteArray &bytes)
{
    QHash<QString, Style> styles;
    QXmlStreamReader xml(bytes);
    QString id;
    Style style;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("style")) {
            id = attribute(xml, "styleId");
            style = Style();
        } else if (xml.isStartElement() && xml.name() == QLatin1String("name")) {
            const QString name = attribute(xml, "val").toLower();
            if (name.startsWith(QLatin1String("heading ")))
                style.level = name.mid(8).toInt() - 1;
            style.title = name == QLatin1String("title") || name == QLatin1String("subtitle");
        } else if (xml.isStartElement() && xml.name() == QLatin1String("outlineLvl")) {
            style.level = attribute(xml, "val").toInt();
        } else if (xml.isEndElement() && xml.name() == QLatin1String("style")) {
            styles.insert(id, style);
        }
    }
    return styles;
}

struct Paragraph
{
    int level = -1;
    bool title = false;
    QString text;
};

// Escapes the Markdown marks in plain text.
QString escaped(const QString &text)
{
    QString out;
    for (const QChar c : text) {
        if (c == u'*' || c == u'_' || c == u'\\' || c == u'`')
            out += u'\\';
        out += c;
    }
    return out;
}

// The paragraphs of the body, with the runs as Markdown.
QList<Paragraph> readBody(const QByteArray &bytes, const QHash<QString, Style> &styles)
{
    QList<Paragraph> paragraphs;
    QXmlStreamReader xml(bytes);
    Paragraph paragraph;
    bool inRun = false;
    bool bold = false;
    bool italic = false;
    QString run;
    const auto on = [&xml] {
        const QString value = attribute(xml, "val");
        return value.isEmpty() || (value != QLatin1String("0") && value != QLatin1String("false"));
    };
    const auto endRun = [&] {
        const QString core = run.trimmed();
        if (core.isEmpty() || (!bold && !italic)) {
            paragraph.text += escaped(run);
        } else {
            const QString mark = bold && italic ? QStringLiteral("***")
                : bold                          ? QStringLiteral("**")
                                                : QStringLiteral("*");
            const qsizetype lead = run.indexOf(core);
            paragraph.text
                += run.left(lead) + mark + escaped(core) + mark + run.mid(lead + core.size());
        }
        run.clear();
    };
    while (!xml.atEnd()) {
        xml.readNext();
        const QStringView name = xml.name();
        if (xml.isStartElement()) {
            if (name == QLatin1String("p")) {
                paragraph = Paragraph();
            } else if (name == QLatin1String("pStyle")) {
                const Style style
                    = styles.value(attribute(xml, "styleId").isEmpty() ? attribute(xml, "val")
                                                                       : attribute(xml, "styleId"));
                paragraph.level = style.level;
                paragraph.title = style.title;
            } else if (name == QLatin1String("outlineLvl") && !inRun) {
                paragraph.level = attribute(xml, "val").toInt();
            } else if (name == QLatin1String("r")) {
                inRun = true;
                bold = false;
                italic = false;
            } else if (inRun && name == QLatin1String("b")) {
                bold = on();
            } else if (inRun && name == QLatin1String("i")) {
                italic = on();
            } else if (inRun && name == QLatin1String("t")) {
                run += xml.readElementText();
            } else if (inRun && name == QLatin1String("tab")) {
                run += u' ';
            } else if (inRun && name == QLatin1String("br")
                       && attribute(xml, "type") != QLatin1String("page")) {
                run += QChar(0x2028);
            }
        } else if (xml.isEndElement()) {
            if (name == QLatin1String("r")) {
                endRun();
                inRun = false;
            } else if (name == QLatin1String("p")) {
                // Word puts a heading level of 9 on body text: no heading.
                if (paragraph.level > 8)
                    paragraph.level = -1;
                paragraph.text.replace(QChar(0x2028), QStringLiteral("\\\n"));
                paragraphs.append(paragraph);
            }
        }
    }
    return paragraphs;
}

bool isSceneBreak(const QString &text)
{
    const QString mark = text.trimmed().remove(u' ').remove(u'\\');
    return mark == QLatin1String("***") || mark == QLatin1String("#") || mark == QLatin1String("*");
}

} // namespace

namespace DocxImport {

QList<LoadError> importFile(const QString &docx, const QString &target, const QString &title)
{
    QZipReader zip(docx);
    const QByteArray body = zip.fileData(QStringLiteral("word/document.xml"));
    if (!zip.isReadable() || body.isEmpty())
        return { { docx, 0, QStringLiteral("this file is not a DOCX document") } };
    const QList<Paragraph> paragraphs
        = readBody(body, readStyles(zip.fileData(QStringLiteral("word/styles.xml"))));

    // The chapters start at the headings of the highest level in the file.
    int top = -1;
    for (const Paragraph &paragraph : paragraphs) {
        if (paragraph.level >= 0 && !paragraph.title && !paragraph.text.trimmed().isEmpty()
            && (top < 0 || paragraph.level < top))
            top = paragraph.level;
    }
    QList<MarkdownImport::Chapter> chapters { { QString(), { } } };
    QStringList text;
    const auto endScene = [&] {
        if (text.isEmpty())
            return;
        MarkdownImport::Chapter &chapter = chapters.last();
        const QString name = chapter.title.isEmpty() ? QStringLiteral("Opening") : chapter.title;
        chapter.scenes.append(
            { chapter.scenes.isEmpty()
                  ? name
                  : QStringLiteral("%1, part %2").arg(name).arg(chapter.scenes.size() + 1),
              text.join(QStringLiteral("\n\n")) + u'\n' });
        text.clear();
    };
    for (const Paragraph &paragraph : paragraphs) {
        const QString line = paragraph.text.trimmed();
        if (line.isEmpty() || paragraph.title)
            continue;
        if (top >= 0 && paragraph.level == top) {
            endScene();
            chapters.append({ QString(line).remove(u'\\').remove(u'*'), { } });
        } else if (isSceneBreak(line)) {
            endScene();
        } else if (paragraph.level >= 0) {
            // A lower heading stays as a Markdown heading in the scene.
            text.append(QString(paragraph.level - top + 1, u'#') + u' ' + line);
        } else {
            text.append(line);
        }
    }
    endScene();
    if (chapters.first().scenes.isEmpty())
        chapters.removeFirst();
    if (chapters.isEmpty())
        return { { docx, 0, QStringLiteral("the document has no text") } };
    // A chapter with no text keeps an empty scene, so it stays in the binder.
    for (MarkdownImport::Chapter &chapter : chapters) {
        if (chapter.scenes.isEmpty())
            chapter.scenes.append({ chapter.title, QString() });
    }
    return MarkdownImport::build(target, title, chapters);
}

} // namespace DocxImport
