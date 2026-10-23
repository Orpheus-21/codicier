#pragma once

#include <QString>
#include <QVariantList>
#include <QVariantMap>

// The bibliography of a project: sources.json in the project folder, an
// array of CSL-JSON items, the format that Zotero and pandoc read. The app
// edits the common fields and keeps every other field of an item as it is.
//
// A source as a form has the keys id, type, title, authors, year,
// publisher, container, page, url, and doi, all strings. authors holds the
// names as "Family, Given; Family, Given". A name with no comma is a
// literal name, such as an organisation. list() adds label, a short name
// such as "Smith 2020, Salt".
namespace Sources {

// The sources of the project, in file order. A missing file gives none.
// A file that is not a CSL-JSON array also gives none, and sets error.
QVariantList list(const QString &folder, QString *error = nullptr);

// Adds the source of form, or with the id of an existing source, changes
// it. A new source with no id gets a key from its first author and year.
// Sets id to the key. Returns an error text, or "".
QString save(const QString &folder, const QVariantMap &form, QString *id = nullptr);

// Removes the source with the key id. Returns an error text, or "".
QString remove(const QString &folder, const QString &id);

// The quote bank: quotes.json, an array of quotes with the keys id, text,
// source (the key of a source), and page. A missing file gives none.
QVariantList quotes(const QString &folder, QString *error = nullptr);

// Adds a quote, or with the id of an existing quote, changes it. A quote
// needs its text. Sets id. Returns an error text, or "".
QString saveQuote(const QString &folder, const QVariantMap &quote, QString *id = nullptr);

// Removes the quote with id. Returns an error text, or "".
QString removeQuote(const QString &folder, const QString &id);

} // namespace Sources
