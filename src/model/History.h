#pragma once

#include <QDate>
#include <QMap>
#include <QString>

// The writing history of a project in history.toml: the net words that the
// author added to the manuscript on each day. The file is in the project
// folder, because the app cannot build it again.
class History
{
public:
    // Reads history.toml of the project. A missing or bad file is an empty
    // history.
    explicit History(QString projectFolder);

    // Adds words to a day. A negative count is a day of cuts.
    void add(const QDate &day, int words);
    int words(const QDate &day) const { return days.value(day); }
    // The days in a row with words, up to today. A day with no words yet
    // today does not end the streak.
    int streak(const QDate &today) const;
    // Writes history.toml if the history changed. Returns false on an error.
    bool save();

private:
    QString folder;
    QMap<QDate, int> days;
    bool changed = false;
};
