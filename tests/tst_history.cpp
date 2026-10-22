#include "History.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class TestHistory : public QObject
{
    Q_OBJECT

private slots:
    void addsSavesAndReadsDays()
    {
        QTemporaryDir dir;
        const QDate today(2026, 10, 20);
        {
            History history(dir.path());
            QVERIFY(history.save());
            // No change writes no file.
            QVERIFY(!QFile::exists(dir.filePath("history.toml")));
            history.add(today, 300);
            history.add(today, -50);
            history.add(today.addDays(-1), 120);
            history.add(today.addDays(-3), 40);
            history.add(today, 0);
            QVERIFY(history.save());
        }
        QFile file(dir.filePath("history.toml"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(),
                 QByteArray("[days]\n2026-10-17 = 40\n2026-10-19 = 120\n2026-10-20 = 250\n"));
        file.close();

        const History again(dir.path());
        QCOMPARE(again.words(today), 250);
        QCOMPARE(again.words(today.addDays(-2)), 0);
        // Today and yesterday; the gap two days ago ends the streak.
        QCOMPARE(again.streak(today), 2);
        // With no words yet today, the streak up to yesterday counts.
        QCOMPARE(again.streak(today.addDays(1)), 2);
        QCOMPARE(again.streak(today.addDays(2)), 0);
    }

    void minutesAreKept()
    {
        QTemporaryDir dir;
        const QDate day(2026, 10, 22);
        {
            History history(dir.path());
            history.addMinute(day);
            history.addMinute(day);
            history.add(day, 120);
            QVERIFY(history.save());
        }
        const History again(dir.path());
        QCOMPARE(again.minutes(day), 2);
        QCOMPARE(again.words(day), 120);
        QCOMPARE(again.minutes(day.addDays(1)), 0);
    }

    void badFileIsAnEmptyHistory()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("history.toml"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[days\nbroken");
        file.close();
        const History history(dir.path());
        QCOMPARE(history.words(QDate(2026, 10, 20)), 0);
        QCOMPARE(history.streak(QDate(2026, 10, 20)), 0);
    }
};

QTEST_GUILESS_MAIN(TestHistory)
#include "tst_history.moc"
