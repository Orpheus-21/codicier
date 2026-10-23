#include "Rtf.h"

#include <QFile>
#include <QtTest>

class TestRtf : public QObject
{
    Q_OBJECT

    static QString md(const char *body)
    {
        return Rtf::toMarkdown(
            QByteArray("{\\rtf1\\ansi\\ansicpg1252{\\fonttbl\\f0\\fnil Palatino;}"
                       "{\\colortbl;\\red0\\green0\\blue0;}\\f0 ")
            + body + "}");
    }

private slots:
    void paragraphs()
    {
        QCOMPARE(md("One.\\\nTwo.\\par Three."), QStringLiteral("One.\n\nTwo.\n\nThree."));
        // Empty paragraphs between paragraphs drop out.
        QCOMPARE(md("One.\\\n\\\nTwo."), QStringLiteral("One.\n\nTwo."));
    }

    void lineBreakIsAHardBreak()
    {
        QCOMPARE(md("Roses\\line violets"), QStringLiteral("Roses\\\nviolets"));
    }

    void boldAndItalic()
    {
        QCOMPARE(md("A \\b bold\\b0  and \\i soft\\i0  word."),
                 QStringLiteral("A **bold** and *soft* word."));
        QCOMPARE(md("{\\i\\b both} end"), QStringLiteral("***both*** end"));
        QCOMPARE(md("\\i x \\plain y"), QStringLiteral("*x* y"));
    }

    void unicodeAndCodePage()
    {
        QCOMPARE(md("Caf\\'e9 \\uc0\\u8212  \\'93q\\'94"), QStringLiteral("Café — “q”"));
        // With \uc1, one fallback character follows and drops out.
        QCOMPARE(md("\\uc1\\u233 e!"), QStringLiteral("é!"));
        QCOMPARE(md("\\u-3913 ?"), QString(QChar(0xf0b7)));
        QCOMPARE(md("\\emdash \\ldblquote x\\rdblquote"), QStringLiteral("—“x”"));
    }

    void escapedCharacters()
    {
        QCOMPARE(md("a \\{b\\} \\\\ c"), QStringLiteral("a {b} \\\\ c"));
        QCOMPARE(md("2*3 and snake_case"), QStringLiteral("2\\*3 and snake\\_case"));
    }

    void noTextGroupsDropOut()
    {
        QCOMPARE(md("{\\*\\expandedcolortbl;;}Kept.{\\pict\\pngblip 89504e47}"),
                 QStringLiteral("Kept."));
        QCOMPARE(md("{\\info{\\title T}}Body"), QStringLiteral("Body"));
    }

    void linkKeepsItsText()
    {
        QCOMPARE(
            md("See {\\field{\\*\\fldinst{HYPERLINK \"scrivcmt://ABC\"}}{\\fldrslt the log}} now."),
            QStringLiteral("See the log now."));
    }

    void scrivenerStyleMarkersGo()
    {
        QCOMPARE(md("<$Scr_H::1>Title<!$Scr_H::1>"), QStringLiteral("Title"));
    }

    void fixtureScene()
    {
        QFile file(QStringLiteral(FIXTURE_DIR "/scriv/Lighthouse.scriv/Files/Data/"
                                              "00000004-0000-4000-8000-000000000004/content.rtf"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(
            Rtf::toMarkdown(file.readAll()),
            QStringLiteral("The lamp had not gone out in forty years. Elin climbed the stairs at "
                           "dusk, as her father had.\n\nAt the top she wrote: *Fair. Wind "
                           "north-west.* Her father’s hand filled the pages **before** hers. "
                           "Café — “closed”."));
    }
};

QTEST_GUILESS_MAIN(TestRtf)
#include "tst_rtf.moc"
