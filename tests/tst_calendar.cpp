#include "Calendar.h"

#include <QTest>

namespace {

// The day number of a date, or a value far from any real date.
qint64 day(const Calendar &calendar, const char *date)
{
    return calendar.dayOf(date).value_or(-1000000000);
}

} // namespace

class TestCalendar : public QObject
{
    Q_OBJECT

private slots:
    void gregorianDates()
    {
        const Calendar calendar;
        QVERIFY(!calendar.isCustom());
        QCOMPARE(calendar.label("1847-03-04"), QStringLiteral("4 March 1847"));
        QCOMPARE(day(calendar, "1847-03-05") - day(calendar, "1847-03-04"), 1);
        QVERIFY(day(calendar, "-44-03-15") < day(calendar, "1-01-01"));
        for (const char *bad : { "1847-02-30", "1847-13-01", "4 March", "", "1847-3" })
            QVERIFY2(!calendar.dayOf(bad), bad);
        QCOMPARE(calendar.label("1847-02-30"), QString());
    }

    void customMonthsAndDays()
    {
        const Calendar calendar({ { "Frostmonth", 40 }, { "Thaw", 20 } });
        QVERIFY(calendar.isCustom());
        QCOMPARE(calendar.label("312-1-40"), QStringLiteral("40 Frostmonth 312"));
        // A year has 60 days: the day after 40 Frostmonth is 1 Thaw.
        QCOMPARE(day(calendar, "312-2-1") - day(calendar, "312-1-40"), 1);
        QCOMPARE(day(calendar, "313-1-1") - day(calendar, "312-1-1"), 60);
        QVERIFY(!calendar.dayOf("312-2-21"));
        QVERIFY(!calendar.dayOf("312-3-1"));
        QVERIFY(!calendar.dayOf("312-0-1"));
    }
};

QTEST_GUILESS_MAIN(TestCalendar)
#include "tst_calendar.moc"
