#include "Calendar.h"

#include <QDate>
#include <QLocale>
#include <QRegularExpression>

#include <utility>

namespace {

struct Parts
{
    int year = 0;
    int month = 0;
    int day = 0;
};

std::optional<Parts> split(const QString &date)
{
    static const QRegularExpression pattern(QStringLiteral("^(-?\\d{1,6})-(\\d{1,2})-(\\d{1,2})$"));
    const QRegularExpressionMatch match = pattern.match(date.trimmed());
    if (!match.hasMatch())
        return std::nullopt;
    return Parts { match.captured(1).toInt(), match.captured(2).toInt(),
                   match.captured(3).toInt() };
}

} // namespace

Calendar::Calendar(QList<Month> months) : own(std::move(months)) { }

std::optional<qint64> Calendar::dayOf(const QString &date) const
{
    const std::optional<Parts> parts = split(date);
    if (!parts)
        return std::nullopt;
    if (own.isEmpty()) {
        const QDate gregorian(parts->year, parts->month, parts->day);
        if (!gregorian.isValid())
            return std::nullopt;
        return gregorian.toJulianDay();
    }
    if (parts->month < 1 || parts->month > own.size() || parts->day < 1
        || parts->day > own.at(parts->month - 1).days)
        return std::nullopt;
    qint64 yearDays = 0;
    qint64 before = 0;
    for (int i = 0; i < own.size(); ++i) {
        yearDays += own.at(i).days;
        if (i < parts->month - 1)
            before += own.at(i).days;
    }
    return parts->year * yearDays + before + parts->day - 1;
}

QString Calendar::label(const QString &date) const
{
    const std::optional<Parts> found = split(date);
    if (!found || !dayOf(date))
        return { };
    const Parts &parts = *found;
    const QString month = own.isEmpty()
        ? QLocale(QLocale::English, QLocale::UnitedKingdom).monthName(parts.month)
        : own.at(parts.month - 1).name;
    return QStringLiteral("%1 %2 %3").arg(parts.day).arg(month).arg(parts.year);
}
