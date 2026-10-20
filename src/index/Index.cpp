#include "Index.h"

#include "ProjectStore.h"

#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>

#include <utility>

namespace {

QString errorOf(const QSqlQuery &query)
{
    return query.lastError().text();
}

// Runs one statement. Returns an error text, or an empty string.
QString run(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery query(db);
    return query.exec(sql) ? QString() : errorOf(query);
}

QString readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return { };
    return QString::fromUtf8(file.readAll());
}

} // namespace

Index::Index(QString projectFolder)
    : folder(std::move(projectFolder)), connection(QUuid::createUuid().toString())
{ }

Index::~Index()
{
    if (QSqlDatabase::contains(connection)) {
        QSqlDatabase::database(connection, false).close();
        QSqlDatabase::removeDatabase(connection);
    }
}

QString Index::path(const QString &projectFolder)
{
    return QDir(projectFolder).filePath(QStringLiteral(".codicier/index.db"));
}

QString Index::open()
{
    if (const QString error = ProjectStore::makeAppFolder(folder); !error.isEmpty())
        return error;

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    db.setDatabaseName(path(folder));
    if (!db.open())
        return db.lastError().text();

    QSqlQuery version(db);
    if (!version.exec(QStringLiteral("PRAGMA user_version")) || !version.next())
        return errorOf(version);
    const int found = version.value(0).toInt();
    // An open query keeps a lock, and DROP TABLE needs the tables free.
    version.finish();
    if (found == schemaVersion)
        return { };
    // A new file, or tables of another version: make the tables again.
    for (const QString &sql : {
             QStringLiteral("DROP TABLE IF EXISTS items"),
             QStringLiteral("DROP TABLE IF EXISTS text"),
             QStringLiteral("CREATE TABLE items (id TEXT PRIMARY KEY, kind TEXT NOT NULL, "
                            "title TEXT NOT NULL, section TEXT NOT NULL, file TEXT NOT NULL, "
                            "words INTEGER NOT NULL)"),
             QStringLiteral("CREATE VIRTUAL TABLE text USING fts5(id UNINDEXED, title, body, "
                            "tokenize = 'unicode61 remove_diacritics 2')"),
             QStringLiteral("PRAGMA user_version = %1").arg(schemaVersion),
         }) {
        if (const QString error = run(db, sql); !error.isEmpty())
            return error;
    }
    return { };
}

namespace {

// Inserts one item and its text. Returns an error text, or an empty string.
QString insertItem(QSqlDatabase &db, const ProjectItem &entry, const QDir &dir)
{
    QSqlQuery item(db);
    QSqlQuery text(db);
    item.prepare(QStringLiteral("INSERT INTO items VALUES (?, ?, ?, ?, ?, ?)"));
    text.prepare(QStringLiteral("INSERT INTO text (id, title, body) VALUES (?, ?, ?)"));
    // A research file is not text, so only its title is searched.
    const bool isText = !entry.file.isEmpty() && !holdsNoText(entry.kind);
    const QString body = isText ? readText(dir.filePath(entry.file)) : QString();
    // A null QString binds as NULL, so an item with no file stores "".
    const QString file = entry.file.isEmpty() ? QStringLiteral("") : entry.file;
    item.bindValue(0, entry.id);
    item.bindValue(1, entry.kind);
    item.bindValue(2, entry.title);
    item.bindValue(3, entry.section);
    item.bindValue(4, file);
    item.bindValue(5, Index::countWords(body));
    text.bindValue(0, entry.id);
    text.bindValue(1, entry.title);
    text.bindValue(2, body);
    if (!item.exec())
        return errorOf(item);
    if (!text.exec())
        return errorOf(text);
    return { };
}

} // namespace

QString Index::rebuild(const Project &project)
{
    QSqlDatabase db = QSqlDatabase::database(connection, false);
    if (!db.isOpen())
        return QStringLiteral("the index is not open");
    if (!db.transaction())
        return db.lastError().text();
    QString error = run(db, QStringLiteral("DELETE FROM items"));
    if (error.isEmpty())
        error = run(db, QStringLiteral("DELETE FROM text"));
    const QDir dir(project.folder);
    for (const ProjectItem &entry : project.items) {
        if (!error.isEmpty())
            break;
        error = insertItem(db, entry, dir);
    }
    if (!error.isEmpty()) {
        db.rollback();
        return error;
    }
    return db.commit() ? QString() : db.lastError().text();
}

QString Index::update(const Project &project, const QString &id)
{
    QSqlDatabase db = QSqlDatabase::database(connection, false);
    if (!db.isOpen())
        return QStringLiteral("the index is not open");
    if (!db.transaction())
        return db.lastError().text();
    QString error;
    for (const QString &table : { QStringLiteral("items"), QStringLiteral("text") }) {
        QSqlQuery remove(db);
        remove.prepare(QStringLiteral("DELETE FROM %1 WHERE id = ?").arg(table));
        remove.addBindValue(id);
        if (error.isEmpty() && !remove.exec())
            error = errorOf(remove);
    }
    // An item that is gone from the project stays out of the index.
    const QDir dir(project.folder);
    for (const ProjectItem &entry : project.items) {
        if (error.isEmpty() && entry.id == id)
            error = insertItem(db, entry, dir);
    }
    if (!error.isEmpty()) {
        db.rollback();
        return error;
    }
    return db.commit() ? QString() : db.lastError().text();
}

QList<SearchHit> Index::search(const QString &query, const QString &open,
                               const QString &close) const
{
    QList<SearchHit> hits;
    // Each word becomes a quoted term, so FTS5 syntax in the query is plain text.
    QStringList terms;
    for (const QString &word : query.split(u' ', Qt::SkipEmptyParts))
        terms.append(u'"' + QString(word).replace(u'"', QStringLiteral("\"\"")) + u'"');
    if (terms.isEmpty())
        return hits;

    QSqlQuery select(QSqlDatabase::database(connection, false));
    select.prepare(QStringLiteral("SELECT id, title, snippet(text, 2, ?, ?, '...', 12) "
                                  "FROM text WHERE text MATCH ? ORDER BY rank"));
    select.addBindValue(open);
    select.addBindValue(close);
    select.addBindValue(terms.join(u' '));
    if (!select.exec())
        return hits;
    while (select.next())
        hits.append(
            { select.value(0).toString(), select.value(1).toString(), select.value(2).toString() });
    return hits;
}

QStringList Index::dump() const
{
    QStringList rows;
    QSqlQuery select(QSqlDatabase::database(connection, false));
    if (!select.exec(QStringLiteral("SELECT items.*, text.body FROM items "
                                    "JOIN text ON text.id = items.id ORDER BY items.id")))
        return rows;
    while (select.next()) {
        QStringList fields;
        for (int i = 0; i < select.record().count(); ++i)
            fields.append(select.value(i).toString());
        rows.append(fields.join(u'|'));
    }
    return rows;
}

qsizetype Index::firstMatch(const QString &text, const QString &query)
{
    // One character folds to one character, so a place in the folded text is
    // the same place in the text.
    const auto fold = [](const QString &in) {
        QString out = in;
        for (QChar &c : out) {
            const QString base = c.decomposition();
            if (!base.isEmpty() && c.decompositionTag() == QChar::Canonical)
                c = base.front();
            c = c.toCaseFolded();
        }
        return out;
    };
    const QStringList words = query.split(u' ', Qt::SkipEmptyParts);
    if (words.isEmpty())
        return -1;
    const QString haystack = fold(text);
    const QString needle = fold(words.first());
    const auto isWord = [&haystack](qsizetype at) {
        return at >= 0 && at < haystack.size() && haystack.at(at).isLetterOrNumber();
    };
    for (qsizetype at = haystack.indexOf(needle); at >= 0; at = haystack.indexOf(needle, at + 1)) {
        if (!isWord(at - 1) && !isWord(at + needle.size()))
            return at;
    }
    return -1;
}

int Index::countWords(const QString &text)
{
    const QList<uint> points = text.toUcs4();
    int words = 0;
    bool inWord = false;
    for (qsizetype i = 0; i < points.size(); ++i) {
        const uint c = points.at(i);
        // A footnote mark such as [^1] is not a word.
        if (c == U'[' && i + 1 < points.size() && points.at(i + 1) == U'^') {
            const qsizetype end = points.indexOf(U']', i);
            if (end > i) {
                i = end;
                inWord = false;
                continue;
            }
        }
        const bool part = QChar::isLetterOrNumber(c) || (inWord && (c == U'\'' || c == U'’'));
        if (part && !inWord)
            ++words;
        inWord = part;
    }
    return words;
}
