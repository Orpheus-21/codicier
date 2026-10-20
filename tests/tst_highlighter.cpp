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

private slots:
    void init()
    {
        document = std::make_unique<QTextDocument>();
        document->setDefaultFont(QFont(QStringLiteral("Serif"), 12));
        // A document sends contentsChange only when it has a layout. The
        // editor makes one; a bare document makes it on the first request.
        document->documentLayout();
        new MarkdownHighlighter(document.get());
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
};

QTEST_MAIN(TestHighlighter)
#include "tst_highlighter.moc"
