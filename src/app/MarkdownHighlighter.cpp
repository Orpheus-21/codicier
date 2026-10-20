#include "MarkdownHighlighter.h"

#include "SpellChecker.h"

#include <QColor>
#include <QFont>

#include <algorithm>

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document), heading(QStringLiteral("^(#{1,6})\\s")),
      strong(QStringLiteral("\\*\\*(?=\\S)(.+?)(?<=\\S)\\*\\*")),
      emphasis(QStringLiteral("(?<![*\\w])[*_](?=[^\\s*_])(.+?)(?<=[^\\s*_])[*_](?![*\\w])")),
      footnote(QStringLiteral("\\[\\^[^\\]\\s]+\\]")),
      // A link to an entity of the codex: [[Mara Vell]].
      link(QStringLiteral("\\[\\[[^\\[\\]\\n]+\\]\\]")),
      word(QStringLiteral("\\p{L}+(?:['’]\\p{L}+)*"),
           QRegularExpression::UseUnicodePropertiesOption)
{
    // The light theme colours until the app sets the theme.
    markFormat.setForeground(QColor(0x70, 0x6b, 0x65));
    strongFormat.setFontWeight(QFont::Bold);
    emphasisFormat.setFontItalic(true);
    footnoteFormat.setForeground(QColor(0x94, 0x55, 0x2a));
    footnoteFormat.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
    linkFormat.setForeground(QColor(0x94, 0x55, 0x2a));
}

void MarkdownHighlighter::setColors(const QColor &mark, const QColor &accent)
{
    if (markFormat.foreground().color() == mark && footnoteFormat.foreground().color() == accent)
        return;
    markFormat.setForeground(mark);
    footnoteFormat.setForeground(accent);
    linkFormat.setForeground(accent);
    rehighlight();
}

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    const QRegularExpressionMatch match = heading.match(text);
    if (match.hasMatch()) {
        // Level 1 is the largest. Each level down is a little smaller.
        const auto level = static_cast<int>(match.capturedLength(1));
        QTextCharFormat format;
        format.setFontWeight(QFont::DemiBold);
        // QML sets the size in pixels, and then the point size is -1.
        const QFont base = document()->defaultFont();
        const double scale = 1.0 + 0.12 * (7 - level);
        if (base.pixelSize() > 0)
            format.setProperty(QTextFormat::FontPixelSize, qRound(base.pixelSize() * scale));
        else
            format.setFontPointSize(base.pointSizeF() * scale);
        setFormat(0, static_cast<int>(text.size()), format);
        format.setForeground(markFormat.foreground());
        setFormat(0, level, format);
        return;
    }
    styleSpans(text, strong, 2, strongFormat);
    styleSpans(text, emphasis, 1, emphasisFormat);
    styleSpans(text, footnote, 0, footnoteFormat);
    styleSpans(text, link, 2, linkFormat);
    checkSpelling(text);
}

void MarkdownHighlighter::setSpellChecker(const SpellChecker *checker)
{
    if (spell == checker)
        return;
    spell = checker;
    rehighlight();
}

void MarkdownHighlighter::checkSpelling(const QString &text)
{
    if (spell == nullptr)
        return;
    // A link names an entity, and a footnote mark is not a word.
    QList<std::pair<qsizetype, qsizetype>> skip;
    for (const QRegularExpression *pattern : { &link, &footnote }) {
        auto matches = pattern->globalMatch(text);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            skip.append({ match.capturedStart(), match.capturedEnd() });
        }
    }
    auto matches = word.globalMatch(text);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        const qsizetype start = match.capturedStart();
        if (std::ranges::any_of(skip, [start](const auto &range) {
                return start >= range.first && start < range.second;
            }))
            continue;
        if (spell->isCorrect(match.captured()))
            continue;
        QTextCharFormat merged = format(static_cast<int>(start));
        merged.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
        merged.setUnderlineColor(linkFormat.foreground().color());
        setFormat(static_cast<int>(start), static_cast<int>(match.capturedLength()), merged);
    }
}

void MarkdownHighlighter::styleSpans(const QString &text, const QRegularExpression &pattern,
                                     int markLength, const QTextCharFormat &body)
{
    auto matches = pattern.globalMatch(text);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        const auto start = static_cast<int>(match.capturedStart());
        const auto length = static_cast<int>(match.capturedLength());
        // Merge, so a span inside a strong span stays strong.
        QTextCharFormat merged = format(start);
        merged.merge(body);
        setFormat(start, length, merged);
        if (markLength > 0) {
            QTextCharFormat mark = merged;
            mark.setForeground(markFormat.foreground());
            setFormat(start, markLength, mark);
            setFormat(start + length - markLength, markLength, mark);
        }
    }
}
