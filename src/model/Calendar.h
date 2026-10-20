#pragma once

#include <QList>
#include <QString>

#include <optional>

// The calendar of story time. A date is written "year-month-day", such as
// "1847-03-04", with a minus sign for a year before year 0. With no months
// of its own, the calendar is the Gregorian one.
class Calendar
{
public:
    struct Month
    {
        QString name;
        int days = 30;
    };

    // The Gregorian calendar.
    Calendar() = default;
    // A calendar with its own months. The year has the sum of their days.
    explicit Calendar(QList<Month> months);

    bool isCustom() const { return !own.isEmpty(); }
    const QList<Month> &months() const { return own; }
    // The number of the day, to sort dates and to place them on a timeline,
    // or nothing for a date that is not valid in this calendar.
    std::optional<qint64> dayOf(const QString &date) const;
    // A date to read, such as "4 March 1847", or "" for a date that is not
    // valid.
    QString label(const QString &date) const;

private:
    QList<Month> own;
};
