#pragma once

#include "Project.h"

#include <QList>
#include <QString>

// One search result.
struct SearchHit
{
    QString id;
    QString title;
    // A short part of the text around the match. The match is between
    // brackets: "the [fourth] of March".
    QString snippet;
};

// The search index in .codicier/index.db. The files are the source of truth,
// so the app can delete the index and build it again at any time.
class Index
{
public:
    // The version of the tables. A database with another version is built again.
    static constexpr int schemaVersion = 1;

    explicit Index(QString projectFolder);
    ~Index();
    Index(const Index &) = delete;
    Index &operator=(const Index &) = delete;

    static QString path(const QString &projectFolder);

    // Opens or makes the database. Returns an error text, or an empty string.
    QString open();
    // Fills the index from the project and its files, in one transaction.
    QString rebuild(const Project &project);
    // Full text search over the titles and the text, best match first.
    QList<SearchHit> search(const QString &query) const;
    // Every row of the items table, in id order, as text. For tests.
    QStringList dump() const;

    // The number of words in a text: runs of letters and digits.
    static int countWords(const QString &text);

private:
    QString folder;
    QString connection;
};
