#include "Sources.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

#include <array>
#include <utility>

namespace {

constexpr const char *sourcesFile = "sources.json";
constexpr const char *quotesFile = "quotes.json";

// Reads the array of a file of the project. A missing file is an empty
// array; a broken file is an error, so a save never writes over it.
bool read(const QString &folder, QJsonArray &array, QString *error, const char *name = sourcesFile)
{
    QFile file(QDir(folder).filePath(QString::fromLatin1(name)));
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr)
            *error = file.errorString();
        return false;
    }
    QJsonParseError parse;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isArray()) {
        if (error != nullptr)
            *error = QStringLiteral("%1 is not a JSON array").arg(name);
        return false;
    }
    array = document.array();
    return true;
}

QString write(const QString &folder, const QJsonArray &array, const char *name = sourcesFile)
{
    QSaveFile out(QDir(folder).filePath(QString::fromLatin1(name)));
    if (!out.open(QIODevice::WriteOnly))
        return out.errorString();
    out.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    return out.commit() ? QString() : out.errorString();
}

QString authorsOf(const QJsonObject &item)
{
    QStringList names;
    for (const QJsonValue each : item.value(QStringLiteral("author")).toArray()) {
        const QJsonObject name = each.toObject();
        if (name.contains(QStringLiteral("literal"))) {
            names.append(name.value(QStringLiteral("literal")).toString());
            continue;
        }
        const QString family = name.value(QStringLiteral("family")).toString();
        const QString given = name.value(QStringLiteral("given")).toString();
        names.append(given.isEmpty() ? family : family + QStringLiteral(", ") + given);
    }
    return names.join(QStringLiteral("; "));
}

QJsonArray authorsFrom(const QString &text)
{
    QJsonArray names;
    for (const QString &each : text.split(u';')) {
        const QString name = each.trimmed();
        if (name.isEmpty())
            continue;
        const qsizetype comma = name.indexOf(u',');
        if (comma < 0)
            names.append(QJsonObject { { QStringLiteral("literal"), name } });
        else
            names.append(
                QJsonObject { { QStringLiteral("family"), name.left(comma).trimmed() },
                              { QStringLiteral("given"), name.mid(comma + 1).trimmed() } });
    }
    return names;
}

QString yearOf(const QJsonObject &item)
{
    const QJsonArray parts = item.value(QStringLiteral("issued"))
                                 .toObject()
                                 .value(QStringLiteral("date-parts"))
                                 .toArray()
                                 .at(0)
                                 .toArray();
    return parts.isEmpty() ? QString() : QString::number(parts.at(0).toInt());
}

// The form fields and their CSL-JSON keys. authors and year are apart.
constexpr std::array<std::pair<const char *, const char *>, 7> plain {
    { { "type", "type" },
      { "title", "title" },
      { "publisher", "publisher" },
      { "container", "container-title" },
      { "page", "page" },
      { "url", "URL" },
      { "doi", "DOI" } }
};

QVariantMap formOf(const QJsonObject &item)
{
    QVariantMap form { { QStringLiteral("id"), item.value(QStringLiteral("id")).toString() },
                       { QStringLiteral("authors"), authorsOf(item) },
                       { QStringLiteral("year"), yearOf(item) } };
    for (const auto &[field, key] : plain)
        form.insert(QString::fromLatin1(field), item.value(QLatin1String(key)).toString());
    // The first family name, or the literal name, and the year.
    const QJsonObject first = item.value(QStringLiteral("author")).toArray().at(0).toObject();
    const QString who = first.contains(QStringLiteral("family"))
        ? first.value(QStringLiteral("family")).toString()
        : first.value(QStringLiteral("literal")).toString();
    QStringList label;
    const QString year = yearOf(item);
    const QString head = who.isEmpty() || year.isEmpty() ? who + year : who + u' ' + year;
    if (!head.isEmpty())
        label.append(head);
    if (!form.value(QStringLiteral("title")).toString().isEmpty())
        label.append(form.value(QStringLiteral("title")).toString());
    form.insert(QStringLiteral("label"), label.join(QStringLiteral(", ")));
    return form;
}

void apply(QJsonObject &item, const QVariantMap &form)
{
    for (const auto &[field, key] : plain) {
        const QString value = form.value(QString::fromLatin1(field)).toString().trimmed();
        if (value.isEmpty())
            item.remove(QLatin1String(key));
        else
            item.insert(QLatin1String(key), value);
    }
    if (!item.contains(QStringLiteral("type")))
        item.insert(QStringLiteral("type"), QStringLiteral("book"));
    const QJsonArray authors = authorsFrom(form.value(QStringLiteral("authors")).toString());
    if (authors.isEmpty())
        item.remove(QStringLiteral("author"));
    else
        item.insert(QStringLiteral("author"), authors);
    bool ok = false;
    const int year = form.value(QStringLiteral("year")).toString().trimmed().toInt(&ok);
    if (ok)
        item.insert(
            QStringLiteral("issued"),
            QJsonObject { { QStringLiteral("date-parts"), QJsonArray { QJsonArray { year } } } });
    else
        item.remove(QStringLiteral("issued"));
}

// A key such as "smith2020": the first family name or title word in lower
// case ASCII, and the year. A taken key gets a letter: smith2020a.
QString keyFor(const QJsonObject &item, const QJsonArray &array)
{
    const QJsonObject first = item.value(QStringLiteral("author")).toArray().at(0).toObject();
    QString base = first.value(QStringLiteral("family")).toString();
    if (base.isEmpty())
        base = first.value(QStringLiteral("literal")).toString();
    if (base.isEmpty())
        base = item.value(QStringLiteral("title")).toString();
    base = base.normalized(QString::NormalizationForm_KD).toLower();
    base.remove(QRegularExpression(QStringLiteral("[^a-z0-9]")));
    base = base.left(20);
    if (base.isEmpty())
        base = QStringLiteral("source");
    base += yearOf(item);
    QStringList taken;
    for (const QJsonValue each : array)
        taken.append(each.toObject().value(QStringLiteral("id")).toString());
    QString key = base;
    for (char letter = 'a'; taken.contains(key) && letter <= 'z'; ++letter)
        key = base + QLatin1Char(letter);
    for (int n = 2; taken.contains(key); ++n)
        key = base + QString::number(n);
    return key;
}

} // namespace

namespace Sources {

QVariantList list(const QString &folder, QString *error)
{
    QJsonArray array;
    QVariantList forms;
    if (!read(folder, array, error))
        return forms;
    for (const QJsonValue each : std::as_const(array))
        forms.append(formOf(each.toObject()));
    return forms;
}

QString save(const QString &folder, const QVariantMap &form, QString *id)
{
    QJsonArray array;
    QString error;
    if (!read(folder, array, &error))
        return error;
    const QString key = form.value(QStringLiteral("id")).toString();
    for (qsizetype i = 0; !key.isEmpty() && i < array.size(); ++i) {
        QJsonObject item = array.at(i).toObject();
        if (item.value(QStringLiteral("id")).toString() != key)
            continue;
        apply(item, form);
        array.replace(i, item);
        if (id != nullptr)
            *id = key;
        return write(folder, array);
    }
    QJsonObject item;
    apply(item, form);
    const QString made = key.isEmpty() ? keyFor(item, array) : key;
    item.insert(QStringLiteral("id"), made);
    array.append(item);
    if (id != nullptr)
        *id = made;
    return write(folder, array);
}

QVariantList quotes(const QString &folder, QString *error)
{
    QJsonArray array;
    QVariantList list;
    if (!read(folder, array, error, quotesFile))
        return list;
    for (const QJsonValue each : std::as_const(array))
        list.append(each.toObject().toVariantMap());
    return list;
}

QString saveQuote(const QString &folder, const QVariantMap &quote, QString *id)
{
    QJsonArray array;
    QString error;
    if (!read(folder, array, &error, quotesFile))
        return error;
    QJsonObject item;
    for (const char *key : { "text", "source", "page" }) {
        const QString value = quote.value(QString::fromLatin1(key)).toString().trimmed();
        if (!value.isEmpty())
            item.insert(QLatin1String(key), value);
    }
    if (!item.contains(QStringLiteral("text")))
        return QStringLiteral("a quote needs its words");
    QString key = quote.value(QStringLiteral("id")).toString();
    if (key.isEmpty())
        key = QStringLiteral("q") + QUuid::createUuid().toString(QUuid::Id128).left(8);
    item.insert(QStringLiteral("id"), key);
    qsizetype at = 0;
    while (at < array.size()
           && array.at(at).toObject().value(QStringLiteral("id")).toString() != key)
        ++at;
    if (at < array.size())
        array.replace(at, item);
    else
        array.append(item);
    if (id != nullptr)
        *id = std::move(key);
    return write(folder, array, quotesFile);
}

QString removeQuote(const QString &folder, const QString &id)
{
    QJsonArray array;
    QString error;
    if (!read(folder, array, &error, quotesFile))
        return error;
    for (qsizetype i = 0; i < array.size(); ++i) {
        if (array.at(i).toObject().value(QStringLiteral("id")).toString() == id) {
            array.removeAt(i);
            return write(folder, array, quotesFile);
        }
    }
    return QStringLiteral("no quote has the id %1").arg(id);
}

QString remove(const QString &folder, const QString &id)
{
    QJsonArray array;
    QString error;
    if (!read(folder, array, &error))
        return error;
    for (qsizetype i = 0; i < array.size(); ++i) {
        if (array.at(i).toObject().value(QStringLiteral("id")).toString() == id) {
            array.removeAt(i);
            return write(folder, array);
        }
    }
    return QStringLiteral("no source has the key %1").arg(id);
}

} // namespace Sources
