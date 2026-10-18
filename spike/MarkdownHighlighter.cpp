#include "MarkdownHighlighter.h"

#include <QColor>
#include <QFont>

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document), heading(QStringLiteral("^(#{1,6})\\s")),
      strong(QStringLiteral("\\*\\*(?=\\S)(.+?)(?<=\\S)\\*\\*")),
      emphasis(QStringLiteral("(?<![*\\w])[*_](?=[^\\s*_])(.+?)(?<=[^\\s*_])[*_](?![*\\w])")),
      footnote(QStringLiteral("\\[\\^[^\\]\\s]+\\]"))
{
    markFormat.setForeground(QColor(0xb5, 0xa9, 0x96));
    strongFormat.setFontWeight(QFont::Bold);
    emphasisFormat.setFontItalic(true);
    footnoteFormat.setForeground(QColor(0x9a, 0x5b, 0x2e));
    footnoteFormat.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
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
