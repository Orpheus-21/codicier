#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Exports the manuscript to Markdown, DOCX, or EPUB. chapters is the list of
// ProjectSession::printChapters, and book is ProjectSession::bookInfo. The
// app writes DOCX and EPUB itself, as zips of XML, so no library is needed.
// Only bold, italic, headings, and the scene breaks carry over.
class Exporter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    // format is "markdown", "docx", or "epub". Returns an error text, or "".
    Q_INVOKABLE QString write(const QString &format, const QVariantList &chapters,
                              const QVariantMap &book, const QUrl &file) const;
};
