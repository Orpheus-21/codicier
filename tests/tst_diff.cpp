#include "Diff.h"

#include <QTest>

namespace {

using Diff::Segment;

// The diff as text: [-removed-] and {+added+}.
QString render(const QList<Segment> &segments)
{
    QString out;
    for (const Segment &segment : segments) {
        if (segment.kind == Segment::Removed)
            out += "[-" + segment.text + "-]";
        else if (segment.kind == Segment::Added)
            out += "{+" + segment.text + "+}";
        else
            out += segment.text;
    }
    return out;
}

QString side(const QList<Segment> &segments, Segment::Kind skip)
{
    QString out;
    for (const Segment &segment : segments) {
        if (segment.kind != skip)
            out += segment.text;
    }
    return out;
}

} // namespace

class TestDiff : public QObject
{
    Q_OBJECT

private slots:
    void cases_data()
    {
        QTest::addColumn<QString>("before");
        QTest::addColumn<QString>("after");
        QTest::addColumn<QString>("expected");
        QTest::newRow("same") << "The river." << "The river." << "The river.";
        QTest::newRow("both empty") << "" << "" << "";
        QTest::newRow("all new") << "" << "New text." << "{+New text.+}";
        QTest::newRow("all gone") << "Old text." << "" << "[-Old text.-]";
        QTest::newRow("one word") << "The quiet river." << "The cold river."
                                  << "The [-quiet-]{+cold+} river.";
        QTest::newRow("added word") << "The river went on." << "The long river went on."
                                    << "The {+long +}river went on.";
        QTest::newRow("removed word") << "The long river went on." << "The river went on."
                                      << "The [-long -]river went on.";
        QTest::newRow("apostrophe stays in the word") << "She didn't know." << "She did know."
                                                      << "She [-didn't-]{+did+} know.";
        QTest::newRow("punctuation")
            << "Yes, she said." << "Yes; she said." << "Yes[-,-]{+;+} she said.";
        QTest::newRow("accents") << "Café crème." << "Café noir." << "Café [-crème-]{+noir+}.";
    }

    void cases()
    {
        QFETCH(QString, before);
        QFETCH(QString, after);
        QFETCH(QString, expected);
        const QList<Segment> segments = Diff::words(before, after);
        QCOMPARE(render(segments), expected);
        // Both sides rebuild from the segments.
        QCOMPARE(side(segments, Segment::Added), before);
        QCOMPARE(side(segments, Segment::Removed), after);
    }

    void longRewriteFallsBackToBlocks()
    {
        QString before;
        QString after;
        for (int i = 0; i < 3000; ++i) {
            before += QStringLiteral("old%1 ").arg(i);
            after += QStringLiteral("new%1 ").arg(i);
        }
        const QList<Segment> segments = Diff::words("Start " + before, "Start " + after);
        // The common start and the common last space stay, and the middle is
        // one removed and one added block.
        QCOMPARE(segments.size(), 4);
        QCOMPARE(segments.at(0), (Segment { Segment::Same, "Start " }));
        QCOMPARE(segments.at(1).kind, Segment::Removed);
        QCOMPARE(segments.at(2).kind, Segment::Added);
        QCOMPARE(segments.at(3), (Segment { Segment::Same, " " }));
        QCOMPARE(side(segments, Segment::Added), "Start " + before);
    }
};

QTEST_GUILESS_MAIN(TestDiff)
#include "tst_diff.moc"
