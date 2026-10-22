#pragma once

#include <QDate>
#include <QDateTime>
#include <QMap>
#include <QString>

// The writing history of a project in history.toml: the net words that the
// author added to the manuscript on each day, and the minutes of writing. The file is in the
// project folder, because the app cannot build it again.
class History
{
public:
    // Reads history.toml of the project. A missing or bad file is an empty
    // history.
    explicit History(QString projectFolder);

    // Adds words to a day. A negative count is a day of cuts.
    void add(const QDate &day, int words);
    int words(const QDate &day) const { return days.value(day); }
    // Counts one minute of writing on a day.
    void addMinute(const QDate &day);
    int minutes(const QDate &day) const { return minutesOf.value(day); }
    // Keeps a session: its start and its words. The last 100 stay.
    void addSession(const QDateTime &start, int words);
    // The sessions by start, oldest first.
    const QMap<QDateTime, int> &sessions() const { return sessionsOf; }
    // The days in a row with words, up to today. A day with no words yet
    // today does not end the streak.
    int streak(const QDate &today) const;
    // Writes history.toml if the history changed. Returns false on an error.
    bool save();

private:
    QString folder;
    QMap<QDate, int> days;
    QMap<QDate, int> minutesOf;
    QMap<QDateTime, int> sessionsOf;
    bool changed = false;
};
