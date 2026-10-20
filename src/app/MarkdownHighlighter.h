#pragma once

#include <QColor>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

#include <cstdint>

// Styles Markdown in place: headings, strong and emphasis, footnote marks,
// and links to the codex. The marks stay in the text, in a pale colour, so the file on disk
// is the text on screen.
class SpellChecker;

class MarkdownHighlighter : public QSyntaxHighlighter
{
public:
    explicit MarkdownHighlighter(QTextDocument *document);

    // The colour of the Markdown marks and of the footnote marks. A change
    // styles the whole text again.
    void setColors(const QColor &mark, const QColor &accent);
    // Underlines the words that the checker does not know. Null turns the
    // check off. A change styles the whole text again.
    void setSpellChecker(const SpellChecker *checker);

    // The style checks, as flags. Each marks its words with a soft tint.
    enum StyleCheck : std::uint8_t {
        Repeats = 1,
        Filters = 2,
        Adverbs = 4,
        Passive = 8,
    };
    void setStyleChecks(int checks);

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
    const SpellChecker *spell = nullptr;
    QRegularExpression word;
    // Underlines the unknown words, but not in links and footnote marks.
    void checkSpelling(const QString &text);
    unsigned styleChecks = 0;
    bool has(StyleCheck check) const { return (styleChecks & static_cast<unsigned>(check)) != 0U; }
    QTextCharFormat styleFormat;
    // Tints the words of the style checks that are on.
    void checkStyle(const QString &text);
};
