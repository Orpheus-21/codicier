#include "MarkdownHighlighter.h"
#include "SpellChecker.h"

#include <QAbstractTextDocumentLayout>
#include <QFont>
#include <QTest>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>

#include <memory>

namespace {

// The format the highlighter gave to one character of the first block.
QTextCharFormat formatAt(const QTextDocument &document, int position)
{
    const QTextBlock block = document.firstBlock();
    for (const QTextLayout::FormatRange &range : block.layout()->formats()) {
        if (position >= range.start && position < range.start + range.length)
            return range.format;
    }
    return { };
}

} // namespace

class TestHighlighter : public QObject
{
    Q_OBJECT

    std::unique_ptr<QTextDocument> document;
    MarkdownHighlighter *highlighter = nullptr;

private slots:
    void init()
    {
        document = std::make_unique<QTextDocument>();
        document->setDefaultFont(QFont(QStringLiteral("Serif"), 12));
        // A document sends contentsChange only when it has a layout. The
        // editor makes one; a bare document makes it on the first request.
        document->documentLayout();
        highlighter = new MarkdownHighlighter(document.get());
    }

    void hiddenMarksTakeNoSpaceAndNoColour()
    {
        const auto hidden = [this](int at) {
            const QTextCharFormat mark = formatAt(*document, at);
            return mark.property(QTextFormat::FontPixelSize).toInt() == 1
                && mark.foreground().color().alpha() == 0;
        };
        highlighter->setHideMarks(true);
        // The heading mark hides with the space after it.
        document->setPlainText(QStringLiteral("## Dusk"));
        QVERIFY(hidden(0) && hidden(1) && hidden(2));
        QVERIFY(!hidden(3));
        QCOMPARE(formatAt(*document, 3).fontWeight(), int(QFont::DemiBold));

        document->setPlainText(QStringLiteral("a *b* **c**"));
        for (const int at : { 2, 4, 6, 7, 9, 10 })
            QVERIFY2(hidden(at), qPrintable(QString::number(at)));
        QVERIFY(formatAt(*document, 3).fontItalic() && !hidden(3));
        QCOMPARE(formatAt(*document, 8).fontWeight(), int(QFont::Bold));
        QVERIFY(!hidden(8));
        // The text keeps every mark.
        QCOMPARE(document->toPlainText(), QStringLiteral("a *b* **c**"));
        // A codex link hides its brackets and keeps its name.
        document->setPlainText(QStringLiteral("see [[Elin]]"));
        for (const int at : { 4, 5, 10, 11 })
            QVERIFY2(hidden(at), qPrintable(QString::number(at)));
        QVERIFY(!hidden(6));
        document->setPlainText(QStringLiteral("a *b* **c**"));

        highlighter->setHideMarks(false);
        QVERIFY(!hidden(2));
        QVERIFY(formatAt(*document, 2).foreground() != formatAt(*document, 3).foreground());
    }

    void theShownBlockKeepsItsMarks()
    {
        const auto hiddenIn = [this](int block, int at) {
            const QTextBlock found = document->findBlockByNumber(block);
            for (const QTextLayout::FormatRange &range : found.layout()->formats()) {
                if (at >= range.start && at < range.start + range.length)
                    return range.format.foreground().color().alpha() == 0;
            }
            return false;
        };
        highlighter->setHideMarks(true);
        document->setPlainText(QStringLiteral("a **b**\nc **d**"));
        QVERIFY(hiddenIn(0, 2) && hiddenIn(1, 2));
        highlighter->setShownBlock(0);
        QVERIFY(!hiddenIn(0, 2) && hiddenIn(1, 2));
        highlighter->setShownBlock(1);
        QVERIFY(hiddenIn(0, 2) && !hiddenIn(1, 2));
        highlighter->setShownBlock(-1);
        QVERIFY(hiddenIn(0, 2) && hiddenIn(1, 2));
    }

    void focusDimsOutsideTheParagraphOrSentence()
    {
        const auto dimIn = [this](int block, int at) {
            const QTextBlock found = document->findBlockByNumber(block);
            for (const QTextLayout::FormatRange &range : found.layout()->formats()) {
                if (at >= range.start && at < range.start + range.length)
                    return range.format.foreground().color() == QColor(0x80, 0x70, 0x60);
            }
            return false;
        };
        highlighter->setColors(QColor(0x80, 0x70, 0x60), QColor(0x20, 0x40, 0x90));
        document->setPlainText(QStringLiteral("One. Two here.\nThree."));
        highlighter->setShownBlock(0, 6);
        QVERIFY(!dimIn(1, 0));
        highlighter->setFocusScope(1);
        // The other paragraph is dim, and this one is bright.
        QVERIFY(dimIn(1, 0));
        QVERIFY(!dimIn(0, 6));
        highlighter->setFocusScope(2);
        // The first sentence is dim, the sentence with the cursor is not.
        QVERIFY(dimIn(0, 0));
        QVERIFY(!dimIn(0, 6));
        highlighter->setShownBlock(0, 1);
        QVERIFY(!dimIn(0, 0) && dimIn(0, 6));
        // Scope 3 keeps the words between quotes bright, with no cursor.
        document->setPlainText(QStringLiteral("He said \"Go now\" and left.\nThe sea."));
        highlighter->setShownBlock(-1, 0);
        highlighter->setFocusScope(3);
        QVERIFY(dimIn(0, 0));
        QVERIFY(!dimIn(0, 10));
        QVERIFY(dimIn(1, 0));
        highlighter->setFocusScope(0);
        QVERIFY(!dimIn(1, 0));
    }

    void highlightTintsItsWords()
    {
        document->setPlainText(QStringLiteral("a ==lamp== b"));
        QVERIFY(formatAt(*document, 4).background().color().alpha() > 0);
        QCOMPARE(formatAt(*document, 0).background().style(), Qt::NoBrush);
        // The marks are pale like the other marks.
        QVERIFY(formatAt(*document, 2).foreground() != formatAt(*document, 4).foreground());
        document->setPlainText(QStringLiteral("a == b =="));
        QCOMPARE(formatAt(*document, 2).background().style(), Qt::NoBrush);
    }

    void codexNamesHaveAQuietUnderline()
    {
        highlighter->setNames({ "Mara", "Mara Vell" });
        document->setPlainText(QStringLiteral("Then mara vell said no."));
        QCOMPARE(formatAt(*document, 5).underlineStyle(), QTextCharFormat::SingleUnderline);
        QCOMPARE(formatAt(*document, 12).underlineStyle(), QTextCharFormat::SingleUnderline);
        QCOMPARE(formatAt(*document, 0).underlineStyle(), QTextCharFormat::NoUnderline);
        // A name inside another word does not count.
        document->setPlainText(QStringLiteral("Amarante"));
        QCOMPARE(formatAt(*document, 1).underlineStyle(), QTextCharFormat::NoUnderline);
        highlighter->setNames({ });
        document->setPlainText(QStringLiteral("Mara"));
        QCOMPARE(formatAt(*document, 0).underlineStyle(), QTextCharFormat::NoUnderline);
    }

    void trackedChangesHaveMarks()
    {
        document->setPlainText(QStringLiteral("The dog sat."));
        highlighter->setChanges({ { 4, 3 } }, { 11 });
        QCOMPARE(formatAt(*document, 5).underlineStyle(), QTextCharFormat::SingleUnderline);
        QVERIFY(formatAt(*document, 5).background().color().alpha() > 0);
        QCOMPARE(formatAt(*document, 0).underlineStyle(), QTextCharFormat::NoUnderline);
        // A deletion marks the character before it.
        QCOMPARE(formatAt(*document, 10).underlineStyle(), QTextCharFormat::DotLine);
        highlighter->setChanges({ }, { });
        QCOMPARE(formatAt(*document, 5).underlineStyle(), QTextCharFormat::NoUnderline);
    }

    void headingIsLargerAndBold()
    {
        document->setPlainText(QStringLiteral("## Chapter 1"));
        const QTextCharFormat word = formatAt(*document, 3);
        QCOMPARE(word.fontWeight(), int(QFont::DemiBold));
        QVERIFY(word.fontPointSize() > 12);
        QVERIFY(formatAt(*document, 0).foreground() != word.foreground());
    }

    void headingNeedsSpace()
    {
        document->setPlainText(QStringLiteral("#hashtag"));
        QVERIFY(document->firstBlock().layout()->formats().isEmpty());
    }

    void strongIsBoldWithPaleMarks()
    {
        document->setPlainText(QStringLiteral("a **b** c"));
        QCOMPARE(formatAt(*document, 4).fontWeight(), int(QFont::Bold));
        QVERIFY(formatAt(*document, 2).foreground() != formatAt(*document, 4).foreground());
        QCOMPARE(formatAt(*document, 8).fontWeight(), int(QFont::Normal));
    }

    void emphasisIsItalic()
    {
        document->setPlainText(QStringLiteral("a *b* and _c_"));
        QVERIFY(formatAt(*document, 3).fontItalic());
        QVERIFY(formatAt(*document, 11).fontItalic());
        QVERIFY(!formatAt(*document, 0).fontItalic());
    }

    void wordUnderscoresAreNotEmphasis()
    {
        document->setPlainText(QStringLiteral("snake_case_name and 2*3*4"));
        QVERIFY(document->firstBlock().layout()->formats().isEmpty());
    }

    void emphasisInsideStrongKeepsBoth()
    {
        document->setPlainText(QStringLiteral("**a *b* c**"));
        const QTextCharFormat b = formatAt(*document, 5);
        QVERIFY(b.fontItalic());
        QCOMPARE(b.fontWeight(), int(QFont::Bold));
    }

    void footnoteMarkIsRaised()
    {
        document->setPlainText(QStringLiteral("word[^12] more"));
        QCOMPARE(formatAt(*document, 6).verticalAlignment(), QTextCharFormat::AlignSuperScript);
        QCOMPARE(formatAt(*document, 1).verticalAlignment(), QTextCharFormat::AlignNormal);
    }

    void typedTextIsStyled()
    {
        document->setPlainText(QStringLiteral("a "));
        QTextCursor cursor(document.get());
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(QStringLiteral("*b*"));
        QVERIFY(formatAt(*document, 3).fontItalic());
    }

    void spellCheckerKnowsTheTestWords()
    {
        SpellChecker::searchPaths() = { QStringLiteral(FIXTURE_DIR "/dict") };
        // No en_GB in the folder, so the checker takes the one dictionary there.
        SpellChecker checker("en-GB");
        QCOMPARE(checker.dictionary(), QStringLiteral("en_TEST"));
        QVERIFY(checker.isCorrect("letter"));
        QVERIFY(!checker.isCorrect("lettr"));
        QVERIFY(checker.suggestions("lettr").contains("letter"));
        checker.addWord("Mara");
        QVERIFY(checker.isCorrect("Mara"));

        SpellChecker::searchPaths() = { QStringLiteral("/no/such/folder") };
        SpellChecker none("en_GB");
        QCOMPARE(none.dictionary(), QString());
        QVERIFY(none.isCorrect("lettr"));
        QVERIFY(none.suggestions("lettr").isEmpty());
    }

    void unknownWordsHaveAnUnderline()
    {
        SpellChecker::searchPaths() = { QStringLiteral(FIXTURE_DIR "/dict") };
        SpellChecker checker("en_TEST");
        QTextDocument document;
        document.setPlainText("The lettr said [[Zork Vale]] and[^xq].");
        MarkdownHighlighter highlighter(&document);
        QCOMPARE(formatAt(document, 4).underlineStyle(), QTextCharFormat::NoUnderline);
        highlighter.setSpellChecker(&checker);
        QCOMPARE(formatAt(document, 4).underlineStyle(), QTextCharFormat::SpellCheckUnderline);
        QCOMPARE(formatAt(document, 0).underlineStyle(), QTextCharFormat::NoUnderline);
        // A link and a footnote mark are not checked.
        QCOMPARE(formatAt(document, 18).underlineStyle(), QTextCharFormat::NoUnderline);
        QCOMPARE(formatAt(document, 35).underlineStyle(), QTextCharFormat::NoUnderline);
        highlighter.setSpellChecker(nullptr);
        QCOMPARE(formatAt(document, 4).underlineStyle(), QTextCharFormat::NoUnderline);
    }

    void styleChecksTintTheirWords()
    {
        QTextDocument document;
        //            0         1         2         3         4         5         6
        //            0123456789012345678901234567890123456789012345678901234567890123456
        document.setPlainText("She just ran quickly. The the letter was taken. Only family came.");
        MarkdownHighlighter highlighter(&document);
        const auto tinted = [&document](int position) {
            return formatAt(document, position).hasProperty(QTextFormat::BackgroundBrush);
        };
        QVERIFY(!tinted(4));
        highlighter.setStyleChecks(MarkdownHighlighter::Filters);
        QVERIFY(tinted(4));
        QVERIFY(!tinted(13));
        highlighter.setStyleChecks(MarkdownHighlighter::Adverbs);
        QVERIFY(tinted(13));
        QVERIFY(!tinted(4));
        // "Only" and "family" end in -ly but are not adverbs.
        QVERIFY(!tinted(48));
        QVERIFY(!tinted(53));
        highlighter.setStyleChecks(MarkdownHighlighter::Repeats);
        QVERIFY(tinted(22));
        QVERIFY(tinted(26));
        QVERIFY(!tinted(30));
        highlighter.setStyleChecks(MarkdownHighlighter::Passive);
        QVERIFY(tinted(37));
        QVERIFY(tinted(42));
        QVERIFY(!tinted(30));
        highlighter.setStyleChecks(0);
        QVERIFY(!tinted(37));
    }

    void passRangesTintTheText()
    {
        QTextDocument document;
        document.setPlainText("Old words. New words.");
        MarkdownHighlighter highlighter(&document);
        highlighter.setPasses({ { 11, 10, 2 } }, { QColor("#2a78d6"), QColor("#eb6834") });
        QVERIFY(!formatAt(document, 2).hasProperty(QTextFormat::BackgroundBrush));
        const QColor tint = formatAt(document, 12).background().color();
        QCOMPARE(tint.rgb(), QColor("#eb6834").rgb());
        QVERIFY(tint.alpha() < 255);
        highlighter.setPasses({ }, { });
        QVERIFY(!formatAt(document, 12).hasProperty(QTextFormat::BackgroundBrush));
    }
};

QTEST_MAIN(TestHighlighter)
#include "tst_highlighter.moc"
