#pragma once

#include <QColor>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

// Styles Markdown in place: headings, strong and emphasis, footnote marks,
// and links to the codex. The marks stay in the text, in a pale colour, so the file on disk
// is the text on screen.
class MarkdownHighlighter : public QSyntaxHighlighter
{
public:
    explicit MarkdownHighlighter(QTextDocument *document);

    // The colour of the Markdown marks and of the footnote marks. A change
    // styles the whole text again.
    void setColors(const QColor &mark, const QColor &accent);

protected:
    void highlightBlock(const QString &text) override;

private:
    // Style each match: the whole match with body, the marks with marks.
    void styleSpans(const QString &text, const QRegularExpression &pattern, int markLength,
                    const QTextCharFormat &body);

    QRegularExpression heading;
    QRegularExpression strong;
    QRegularExpression emphasis;
    QRegularExpression footnote;
    QRegularExpression link;
    QTextCharFormat markFormat;
    QTextCharFormat strongFormat;
    QTextCharFormat emphasisFormat;
    QTextCharFormat footnoteFormat;
    QTextCharFormat linkFormat;
};
