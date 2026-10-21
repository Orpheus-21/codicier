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

    // A part of the text that a draft pass wrote: the start and the length
    // in the whole text, and the pass number from 1.
    struct PassRange
    {
        int start = 0;
        int length = 0;
        int pass = 1;
    };
    // Tints each range with the color of its pass. The colors repeat after
    // the last one. No ranges turns the tints off.
    void setPasses(const QList<PassRange> &ranges, const QList<QColor> &colors);
    // Underlines each commented part of the text, as a start and a length
    // in the whole text, with a dotted line.
    void setComments(const QList<std::pair<int, int>> &ranges);
    // True draws the Markdown marks (the # of a heading, the stars of bold
    // and italic) at 1 px with no colour. The text keeps them.
    void setHideMarks(bool hide);
    // The block that shows its marks while the others hide them: the block
    // with the cursor, or -1 for none. offset is the cursor in the block.
    void setShownBlock(int block, int offset = 0);
    // What stays bright while the rest is dim: 0 everything, 1 the
    // paragraph with the cursor, 2 the sentence with the cursor.
    void setFocusScope(int scope);

protected:
    void highlightBlock(const QString &text) override;

private:
    // Style each match: the whole match with body, the marks with marks.
    void styleSpans(const QString &text, const QRegularExpression &pattern, int markLength,
                    const QTextCharFormat &body);

    // The styles of a block, before the dimming.
    void styleBlock(const QString &text);
    // Dims the text outside the focus scope.
    void dimOutsideFocus(const QString &text);
    // The format of a mark: pale, or hidden.
    QTextCharFormat markOver(QTextCharFormat base) const;

    bool hideMarks = false;
    int shownBlock = -1;
    int shownOffset = 0;
    int focusScope = 0;
    // True while highlightBlock styles a block that hides its marks.
    bool hideHere = false;
    QRegularExpression heading;
    QRegularExpression strong;
    QRegularExpression emphasis;
    QRegularExpression footnote;
    QRegularExpression link;
    QRegularExpression highlight;
    QTextCharFormat markFormat;
    QTextCharFormat strongFormat;
    QTextCharFormat emphasisFormat;
    QTextCharFormat footnoteFormat;
    QTextCharFormat highlightFormat;
    QTextCharFormat linkFormat;
    const SpellChecker *spell = nullptr;
    QRegularExpression word;
    // Underlines the unknown words, but not in links and footnote marks.
    void checkSpelling(const QString &text);
    unsigned styleChecks = 0;
    QList<PassRange> passRanges;
    QList<QColor> passColors;
    QList<std::pair<int, int>> commentRanges;
    // Tints the parts of this block that a pass wrote.
    void tintPasses(const QString &text);
    bool has(StyleCheck check) const { return (styleChecks & static_cast<unsigned>(check)) != 0U; }
    QTextCharFormat styleFormat;
    // Tints the words of the style checks that are on.
    void checkStyle(const QString &text);
};
