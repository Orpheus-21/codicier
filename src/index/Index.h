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
    // Reads one item again: its row and its text. An id that is not in the
    // project leaves the index.
    QString update(const Project &project, const QString &id);
    // Full text search over the titles and the text, best match first. The
    // match in a snippet is between the open and close marks.
    QList<SearchHit> search(const QString &query, const QString &open = QStringLiteral("["),
                            const QString &close = QStringLiteral("]")) const;
    // Every row of the items table, in id order, as text. For tests.
    QStringList dump() const;

    // Where the first word of the query starts in the text as a whole word,
    // with no case and no accents, as the index matches. Returns -1 if there
    // is no match. The length of the match is the length of the word.
    static qsizetype firstMatch(const QString &text, const QString &query);

    // The number of words in a text: runs of letters and digits.
    static int countWords(const QString &text);

private:
    QString folder;
    QString connection;
};
